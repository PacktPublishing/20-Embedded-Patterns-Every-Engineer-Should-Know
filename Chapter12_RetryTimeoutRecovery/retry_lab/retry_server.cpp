// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "BinaryReadStream.h"
#include "EpollReactor.h"
#include "MessageFrame.h"
#include "MessageHeader.h"
#include "SignalFdSource.h"
#include "UdpDatagramReceiver.h"
#include "WeatherControlProtocol.h"

#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

struct Options
{
    std::uint16_t port{9100};
};

struct AppContext
{
    bool stopRequested{false};
    std::uint32_t reportingIntervalMilliseconds{1000};
    pbook::UdpDatagramReceiver* udp{nullptr};
};

bool parse_u16(std::string_view text, std::uint16_t& out)
{
    std::string buffer{text};
    char* end = nullptr;
    const unsigned long value = std::strtoul(buffer.c_str(), &end, 10);

    if (end == nullptr || *end != '\0' || value == 0 || value > 65535)
    {
        return false;
    }

    out = static_cast<std::uint16_t>(value);
    return true;
}

void print_usage(const char* argv0)
{
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --port <port>    UDP port to listen on. Default: 9100\n"
        << "  --help           Print this help\n";
}

bool parse_options(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};

        if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            std::exit(0);
        }
        else if (arg == "--port")
        {
            if (i + 1 >= argc || !parse_u16(argv[++i], options.port))
            {
                std::cerr << "Invalid --port value\n";
                return false;
            }
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }

    return true;
}

bool send_datagram(int fd,
                   pbook::ImmutableByteView bytes,
                   const sockaddr_in& destination) noexcept
{
    const ssize_t sent = ::sendto(
        fd,
        bytes.data(),
        bytes.size(),
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));

    return sent == static_cast<ssize_t>(bytes.size());
}

bool send_ack(AppContext& context,
              std::uint32_t transactionId,
              const sockaddr_in& destination) noexcept
{
    if (context.udp == nullptr)
    {
        return false;
    }

    MessageHeaderV1 header{};
    header.payloadEndian = 0u;
    header.serviceId = weather::WeatherControlServiceId;
    header.messageType = static_cast<std::uint16_t>(
        weather::ControlMessageType::Ack);
    header.transactionId = transactionId;

    std::array<std::byte, 64> frameStorage{};
    std::size_t frameSize = 0u;

    const auto status = writeFrameV1(
        pbook::MutableByteView{
            frameStorage.data(),
            frameStorage.size()},
        header,
        pbook::ImmutableByteView{},
        frameSize);

    if (status != MessageFrameStatus::Ok)
    {
        return false;
    }

    return send_datagram(
        context.udp->fd(),
        pbook::ImmutableByteView{
            frameStorage.data(),
            frameSize},
        destination);
}

void on_signal(int signo, void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    std::cout << "Signal " << signo << " received; stopping\n";
    context->stopRequested = true;
}

void on_request(pbook::ImmutableByteView bytes,
                const sockaddr_in& source,
                void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    MessageHeaderV1 header{};
    pbook::ImmutableByteView payload;

    const auto frameStatus = readFrameV1(bytes, header, payload);
    if (frameStatus != MessageFrameStatus::Ok)
    {
        std::cerr << "Ignoring invalid BDS request\n";
        return;
    }

    if (header.serviceId != weather::WeatherControlServiceId)
    {
        std::cerr << "Ignoring request for another service\n";
        return;
    }

    if (header.transactionId == 0u)
    {
        std::cerr << "Ignoring request without a transaction ID\n";
        return;
    }

    const auto messageType =
        static_cast<weather::ControlMessageType>(header.messageType);

    if (messageType != weather::ControlMessageType::SetReportingInterval)
    {
        std::cerr << "Ignoring unsupported request type\n";
        return;
    }

    weather::SetReportingIntervalRequest request{};
    pbook::BinaryReadStream reader(
        payload,
        payloadEndianFromHeader(header.payloadEndian));

    weather::readSetReportingIntervalRequest(reader, request);
    if (!reader.ok() || reader.remaining() != 0u)
    {
        std::cerr << "Ignoring malformed request payload\n";
        return;
    }

    context->reportingIntervalMilliseconds = request.intervalMilliseconds;

    std::cout
        << "Transaction " << header.transactionId
        << ": reporting interval set to "
        << context->reportingIntervalMilliseconds << " ms"
        << ", idempotent=" << (isIdempotent(header) ? "yes" : "no")
        << "\n";

    if (!send_ack(*context, header.transactionId, source))
    {
        std::cerr << "Failed to send ACK " << header.transactionId;
        if (errno != 0)
        {
            std::cerr << ": errno=" << errno;
        }
        std::cerr << "\n";
        return;
    }

    std::cout << "ACK " << header.transactionId << " sent\n";
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parse_options(argc, argv, options))
    {
        print_usage(argv[0]);
        return 1;
    }

    AppContext context;

    pbook::EpollReactor reactor{8};
    if (!reactor.open())
    {
        std::cerr << "Failed to open epoll reactor\n";
        return 1;
    }

    pbook::UdpDatagramReceiver udp{options.port, on_request, &context};
    if (!udp.open())
    {
        std::cerr << "Failed to open UDP server on port "
                  << options.port << "\n";
        return 1;
    }

    context.udp = &udp;

    if (!reactor.add(udp))
    {
        std::cerr << "Failed to add UDP socket to epoll reactor\n";
        return 1;
    }

    pbook::SignalFdSource signalSource{
        std::initializer_list<int>{SIGINT, SIGTERM},
        on_signal,
        &context};

    if (!signalSource.open() || !reactor.add(signalSource))
    {
        std::cerr << "Failed to configure signal handling\n";
        return 1;
    }

    std::cout
        << "Chapter 12 retry server listening on UDP port "
        << udp.local_port() << "\n";

    while (!context.stopRequested)
    {
        if (reactor.poll_once(-1) < 0)
        {
            std::cerr << "epoll wait failed\n";
            return 1;
        }
    }

    return 0;
}
