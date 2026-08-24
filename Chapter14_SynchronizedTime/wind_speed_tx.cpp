// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "SynchronizedMeasurementProtocol.h"

#include "ImmutableByteView.h"
#include "MessageFrame.h"
#include "MessageHeader.h"
#include "MutableByteView.h"

#include <arpa/inet.h>
#include <netinet/in.h>
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
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace
{

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
        << " [destination-ip] [port] [period-ms]\n\n"
        << "Defaults:\n"
        << "  destination-ip  192.168.56.15\n"
        << "  port            45014\n"
        << "  period-ms       100\n";
}

} // namespace

int main(int argc, char* argv[])
{
    std::cout << std::unitbuf;

    const char* destinationIp = "192.168.56.15";
    int port = 45014;
    int periodMs = 100;

    if (argc > 1)
    {
        destinationIp = argv[1];
    }
    if (argc > 2 && !parsePositiveInt(argv[2], port))
    {
        printUsage(argv[0]);
        return 2;
    }
    if (argc > 3 && !parsePositiveInt(argv[3], periodMs))
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

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(static_cast<std::uint16_t>(port));

    if (::inet_pton(AF_INET, destinationIp, &destination.sin_addr) != 1)
    {
        std::cerr << "Invalid destination IP: " << destinationIp << '\n';
        ::close(socketFd);
        return 2;
    }

    std::cout << "Chronos wind-speed simulator -> "
              << destinationIp << ':' << port
              << " every " << periodMs << " ms\n";
    std::cout << "PTP-synchronized system_clock supplies eventTime.\n\n";

    std::uint32_t sequence = 0;
    auto nextSample = std::chrono::steady_clock::now();
    const auto period = std::chrono::milliseconds{periodMs};

    while (!stopRequested)
    {
        nextSample += period;
        std::this_thread::sleep_until(nextSample);

        if (stopRequested)
        {
            break;
        }

        ++sequence;

        // Deliberately simple synthetic weather data: a slow sawtooth.
        const double speed =
            10.0 + static_cast<double>(sequence % 40u) * 0.10;

        const ch14::WindSpeed measurement{
            .sequence = sequence,
            .eventTimeNs = ch14::synchronizedTimeNowNs(),
            .metersPerSecond = speed
        };

        std::array<std::byte, 64> payloadStorage{};
        pbook::BinaryWriteStream payloadWriter(
            pbook::MutableByteView(
                payloadStorage.data(), payloadStorage.size()),
            Endianness::Little);

        ch14::writeWindSpeed(payloadWriter, measurement);
        if (!payloadWriter.ok())
        {
            std::cerr << "Failed to encode wind-speed payload\n";
            continue;
        }

        const pbook::ImmutableByteView payload(
            payloadStorage.data(),
            payloadWriter.bytesWritten());

        MessageHeaderV1 header{};
        header.payloadEndian = 0u; // Little endian
        header.serviceId = ch14::SynchronizedMeasurementServiceId;
        header.messageType = static_cast<std::uint16_t>(
            ch14::MeasurementMessageType::WindSpeed);
        header.flags = 0u;

        std::array<std::byte, 128> frameStorage{};
        std::size_t frameSize = 0;

        const auto frameStatus = writeFrameV1(
            pbook::MutableByteView(
                frameStorage.data(), frameStorage.size()),
            header,
            payload,
            frameSize);

        if (frameStatus != MessageFrameStatus::Ok)
        {
            std::cerr << "Failed to build BDS frame\n";
            continue;
        }

        const ssize_t sent = ::sendto(
            socketFd,
            frameStorage.data(),
            frameSize,
            0,
            reinterpret_cast<const sockaddr*>(&destination),
            sizeof(destination));

        if (sent != static_cast<ssize_t>(frameSize))
        {
            perror("sendto");
            continue;
        }

        std::cout << "TX  seq=" << std::setw(5) << measurement.sequence
                  << "  speed=" << std::fixed << std::setprecision(2)
                  << std::setw(5) << measurement.metersPerSecond << " m/s"
                  << "  t=";
        printTime(measurement.eventTimeNs);
        std::cout << '\n';
    }

    ::close(socketFd);
    return 0;
}
