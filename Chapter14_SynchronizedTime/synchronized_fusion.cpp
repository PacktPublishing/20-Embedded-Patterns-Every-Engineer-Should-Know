// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "SynchronizedMeasurementProtocol.h"

#include "BoundedQueue.h"
#include "ImmutableByteView.h"
#include "MessageFrame.h"
#include "MessageHeader.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace
{

constexpr std::size_t QueueCapacity = 64;

volatile std::sig_atomic_t stopRequested = 0;

void onSignal(int)
{
    stopRequested = 1;
}

void printTime(std::int64_t nanosecondsSinceEpoch)
{
    constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000LL;
    constexpr std::int64_t NanosecondsPerMillisecond = 1'000'000LL;

    const auto secondsSinceEpoch =
        nanosecondsSinceEpoch / NanosecondsPerSecond;
    const auto remainderNs =
        nanosecondsSinceEpoch % NanosecondsPerSecond;
    const auto milliseconds = remainderNs / NanosecondsPerMillisecond;

    const std::time_t time = static_cast<std::time_t>(secondsSinceEpoch);
    std::tm tm{};
    ::gmtime_r(&time, &tm);

    std::cout << std::put_time(&tm, "%H:%M:%S")
              << '.' << std::setw(3) << std::setfill('0')
              << milliseconds
              << std::setfill(' ');
}

bool parsePositiveInt(const char* text, int& value)
{
    if (text == nullptr)
    {
        return false;
    }

    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0 || parsed > 65535)
    {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

void printUsage(std::string_view program)
{
    std::cout
        << "Usage: " << program
        << " [udp-port] [match-window-ms] [local-period-ms]\n\n"
        << "Defaults:\n"
        << "  udp-port         45014\n"
        << "  match-window-ms  40\n"
        << "  local-period-ms  333\n";
}

std::int64_t absoluteDifference(
    std::int64_t lhs,
    std::int64_t rhs) noexcept
{
    return lhs >= rhs ? lhs - rhs : rhs - lhs;
}

void receiverLoop(
    int socketFd,
    BoundedQueue<ch14::Measurement, QueueCapacity>& remoteQueue)
{
    std::array<std::byte, 512> receiveBuffer{};

    pollfd descriptor{};
    descriptor.fd = socketFd;
    descriptor.events = POLLIN;

    while (!stopRequested)
    {
        descriptor.revents = 0;
        const int pollResult = ::poll(&descriptor, 1, 100);

        if (pollResult < 0)
        {
            if (stopRequested)
            {
                return;
            }
            perror("poll");
            continue;
        }

        if (pollResult == 0 || (descriptor.revents & POLLIN) == 0)
        {
            continue;
        }

        const ssize_t bytesReceived = ::recvfrom(
            socketFd,
            receiveBuffer.data(),
            receiveBuffer.size(),
            0,
            nullptr,
            nullptr);

        if (bytesReceived <= 0)
        {
            if (!stopRequested)
            {
                perror("recvfrom");
            }
            continue;
        }

        MessageHeaderV1 header{};
        pbook::ImmutableByteView payload;

        const auto frameStatus = readFrameV1(
            pbook::ImmutableByteView(
                receiveBuffer.data(),
                static_cast<std::size_t>(bytesReceived)),
            header,
            payload);

        if (frameStatus != MessageFrameStatus::Ok)
        {
            continue;
        }

        if (header.serviceId != ch14::SynchronizedMeasurementServiceId ||
            header.messageType != static_cast<std::uint16_t>(
                ch14::MeasurementMessageType::WindSpeed))
        {
            continue;
        }

        pbook::BinaryReadStream reader(
            payload,
            payloadEndianFromHeader(header.payloadEndian));

        ch14::WindSpeed speed{};
        ch14::readWindSpeed(reader, speed);

        if (!reader.ok() || reader.remaining() != 0u)
        {
            continue;
        }

        // One producer (this receiver) and one consumer (fusion thread).
        // A full queue drops the newest measurement rather than blocking input.
        remoteQueue.tryPush(ch14::Measurement{speed});
    }
}

void localAcquisitionLoop(
    int periodMs,
    BoundedQueue<ch14::Measurement, QueueCapacity>& localQueue)
{
    std::uint32_t sequence = 0;
    auto nextSample = std::chrono::steady_clock::now();
    const auto period = std::chrono::milliseconds{periodMs};

    while (!stopRequested)
    {
        nextSample += period;
        std::this_thread::sleep_until(nextSample);

        if (stopRequested)
        {
            return;
        }

        ++sequence;

        const double direction =
            220.0 + static_cast<double>(sequence % 30u) * 0.5;

        const ch14::WindDirection measurement{
            .sequence = sequence,
            .eventTimeNs = ch14::synchronizedTimeNowNs(),
            .degrees = direction
        };

        // One producer (this acquisition thread) and one consumer (fusion).
        localQueue.tryPush(ch14::Measurement{measurement});
    }
}

void fusionLoop(
    int matchWindowMs,
    BoundedQueue<ch14::Measurement, QueueCapacity>& remoteQueue,
    BoundedQueue<ch14::Measurement, QueueCapacity>& localQueue)
{
    const std::int64_t matchWindowNs =
        static_cast<std::int64_t>(matchWindowMs) * 1'000'000LL;

    std::optional<ch14::WindSpeed> latestSpeed;
    std::optional<ch14::WindDirection> latestDirection;

    while (!stopRequested)
    {
        // Keep only the newest remote wind-speed measurement available.
        while (auto item = remoteQueue.tryPop())
        {
            if (const auto* speed = std::get_if<ch14::WindSpeed>(&*item))
            {
                latestSpeed = *speed;
            }
        }

        auto localItem = localQueue.tryPop();
        if (!localItem)
        {
            std::this_thread::sleep_for(1ms);
            continue;
        }

        if (const auto* direction =
                std::get_if<ch14::WindDirection>(&*localItem))
        {
            latestDirection = *direction;
        }
        else
        {
            continue;
        }

        if (!latestSpeed || !latestDirection)
        {
            std::cout << "Waiting for first remote wind-speed measurement...\n";
            continue;
        }

        const std::int64_t deltaNs = absoluteDifference(
            latestSpeed->eventTimeNs,
            latestDirection->eventTimeNs);

        const double deltaMs =
            static_cast<double>(deltaNs) / 1'000'000.0;

        const bool match = deltaNs <= matchWindowNs;

        std::cout << "REMOTE  speed=" << std::fixed << std::setprecision(2)
                  << std::setw(5) << latestSpeed->metersPerSecond << " m/s"
                  << "  t=";
        printTime(latestSpeed->eventTimeNs);
        std::cout << "  seq=" << latestSpeed->sequence << '\n';

        std::cout << "LOCAL   dir  =" << std::fixed << std::setprecision(1)
                  << std::setw(5) << latestDirection->degrees << " deg"
                  << "  t=";
        printTime(latestDirection->eventTimeNs);
        std::cout << "  seq=" << latestDirection->sequence << '\n';

        std::cout << "FUSION  delta=" << std::fixed << std::setprecision(3)
                  << std::setw(7) << deltaMs << " ms  "
                  << (match ? "MATCH" : "MISS") << "\n\n";
    }
}

} // namespace

int main(int argc, char* argv[])
{
    std::cout << std::unitbuf;

    int udpPort = 45014;
    int matchWindowMs = 40;
    int localPeriodMs = 333;

    if (argc > 1 && !parsePositiveInt(argv[1], udpPort))
    {
        printUsage(argv[0]);
        return 2;
    }
    if (argc > 2 && !parsePositiveInt(argv[2], matchWindowMs))
    {
        printUsage(argv[0]);
        return 2;
    }
    if (argc > 3 && !parsePositiveInt(argv[3], localPeriodMs))
    {
        printUsage(argv[0]);
        return 2;
    }
    if (argc > 4)
    {
        printUsage(argv[0]);
        return 2;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    const int socketFd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd < 0)
    {
        perror("socket");
        return 1;
    }

    const int reuseAddress = 1;
    ::setsockopt(
        socketFd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuseAddress,
        sizeof(reuseAddress));

    sockaddr_in localAddress{};
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddress.sin_port = htons(static_cast<std::uint16_t>(udpPort));

    if (::bind(
            socketFd,
            reinterpret_cast<const sockaddr*>(&localAddress),
            sizeof(localAddress)) < 0)
    {
        perror("bind");
        ::close(socketFd);
        return 1;
    }

    BoundedQueue<ch14::Measurement, QueueCapacity> remoteQueue;
    BoundedQueue<ch14::Measurement, QueueCapacity> localQueue;

    std::cout << "Dev synchronized-fusion application\n"
              << "UDP wind speed:   port " << udpPort << '\n'
              << "Local direction:  every " << localPeriodMs << " ms\n"
              << "Match window:     +/- " << matchWindowMs << " ms\n"
              << "Both event timestamps use PTP-disciplined system_clock.\n\n";

    std::thread receiver(
        receiverLoop,
        socketFd,
        std::ref(remoteQueue));

    std::thread localAcquisition(
        localAcquisitionLoop,
        localPeriodMs,
        std::ref(localQueue));

    std::thread fusion(
        fusionLoop,
        matchWindowMs,
        std::ref(remoteQueue),
        std::ref(localQueue));

    while (!stopRequested)
    {
        std::this_thread::sleep_for(100ms);
    }

    receiver.join();
    localAcquisition.join();
    fusion.join();

    ::close(socketFd);
    return 0;
}
