// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
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

enum class ServerMode
{
    Normal,
    DropFirstAck,
    BusyOnce,
    Silent
};

struct Options
{
    std::uint16_t port{9100};
    ServerMode mode{ServerMode::Normal};
};

struct AppContext
{
    bool stopRequested{false};
    std::uint32_t reportingIntervalMilliseconds{1000};

    bool hasCompletedTransaction{false};
    std::uint32_t completedTransactionId{};
    bool firstAckDropped{false};
    bool busyNackSent{false};

    ServerMode mode{ServerMode::Normal};
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

bool parse_mode(std::string_view text, ServerMode& mode) noexcept
{
    if (text == "normal")
    {
        mode = ServerMode::Normal;
        return true;
    }

    if (text == "drop-first-ack")
    {
        mode = ServerMode::DropFirstAck;
        return true;
    }

    if (text == "busy-once")
    {
        mode = ServerMode::BusyOnce;
        return true;
    }

    if (text == "silent")
    {
        mode = ServerMode::Silent;
        return true;
    }

    return false;
}

const char* mode_name(ServerMode mode) noexcept
{
    switch (mode)
    {
        case ServerMode::Normal:
            return "normal";
        case ServerMode::DropFirstAck:
            return "drop-first-ack";
        case ServerMode::BusyOnce:
            return "busy-once";
        case ServerMode::Silent:
            return "silent";
    }

    return "unknown";
}

const char* nack_reason_name(weather::NackReason reason) noexcept
{
    switch (reason)
    {
        case weather::NackReason::Busy:
            return "busy";
        case weather::NackReason::InvalidValue:
            return "invalid value";
        case weather::NackReason::UnsupportedOperation:
            return "unsupported operation";
    }

    return "unknown";
}

void print_usage(const char* argv0)
{
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --port <port>    UDP port to listen on. Default: 9100\n"
        << "  --mode <mode>    normal | drop-first-ack | busy-once | silent\n"
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
        else if (arg == "--mode")
        {
            if (i + 1 >= argc || !parse_mode(argv[++i], options.mode))
            {
                std::cerr << "Invalid --mode value\n";
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

bool send_response_frame(AppContext& context,
                         std::uint32_t transactionId,
                         weather::ControlMessageType messageType,
                         pbook::ImmutableByteView payload,
                         const sockaddr_in& destination) noexcept
{
    if (context.udp == nullptr)
    {
        return false;
    }

    MessageHeaderV1 header{};
    header.payloadEndian = 0u;
    header.serviceId = weather::WeatherControlServiceId;
    header.messageType = static_cast<std::uint16_t>(messageType);
    header.transactionId = transactionId;

    std::array<std::byte, 64> frameStorage{};
    std::size_t frameSize = 0u;

    const auto status = writeFrameV1(
        pbook::MutableByteView{
            frameStorage.data(),
            frameStorage.size()},
        header,
        payload,
        frameSize);

    if (status != MessageFrameStatus::Ok)
    {
        return false;
    }

    return send_datagram(
        context.udp->fd(),
        pbook::ImmutableByteView{frameStorage.data(), frameSize},
        destination);
}

bool send_ack(AppContext& context,
              std::uint32_t transactionId,
              const sockaddr_in& destination) noexcept
{
    return send_response_frame(
        context,
        transactionId,
        weather::ControlMessageType::Ack,
        pbook::ImmutableByteView{},
        destination);
}

bool send_nack(AppContext& context,
               std::uint32_t transactionId,
               weather::NackReason reason,
               const sockaddr_in& destination) noexcept
{
    std::array<std::byte, 8> payloadStorage{};
    pbook::BinaryWriteStream writer(
        pbook::MutableByteView{payloadStorage.data(), payloadStorage.size()},
        Endianness::Little);

    const weather::NackResponse response{.reason = reason};
    weather::writeNackResponse(writer, response);
    if (!writer.ok())
    {
        return false;
    }

    return send_response_frame(
        context,
        transactionId,
        weather::ControlMessageType::Nack,
        pbook::ImmutableByteView{payloadStorage.data(), writer.bytesWritten()},
        destination);
}

void report_send_failure(const char* responseName,
                         std::uint32_t transactionId) noexcept
{
    std::cerr << "Failed to send " << responseName << " " << transactionId;
    if (errno != 0)
    {
        std::cerr << ": errno=" << errno;
    }
    std::cerr << "\n";
}

void send_ack_or_report(AppContext& context,
                        std::uint32_t transactionId,
                        const sockaddr_in& destination) noexcept
{
    if (!send_ack(context, transactionId, destination))
    {
        report_send_failure("ACK", transactionId);
        return;
    }

    std::cout << "ACK " << transactionId << " sent\n";
}

void send_nack_or_report(AppContext& context,
                         std::uint32_t transactionId,
                         weather::NackReason reason,
                         const sockaddr_in& destination) noexcept
{
    if (!send_nack(context, transactionId, reason, destination))
    {
        report_send_failure("NACK", transactionId);
        return;
    }

    std::cout
        << "NACK " << transactionId << " sent: "
        << nack_reason_name(reason) << "\n";
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
        send_nack_or_report(
            *context,
            header.transactionId,
            weather::NackReason::UnsupportedOperation,
            source);
        return;
    }

    // Only one transaction is outstanding in this lab. Remembering the most
    // recently completed transaction is therefore enough to demonstrate
    // duplicate detection without building a transaction cache.
    if (context->hasCompletedTransaction &&
        header.transactionId == context->completedTransactionId)
    {
        std::cout << "Duplicate transaction " << header.transactionId << "\n";
        std::cout << "Operation already completed; not processing again\n";

        if (context->mode == ServerMode::Silent)
        {
            std::cout << "Response suppressed (silent mode)\n";
            return;
        }

        send_ack_or_report(*context, header.transactionId, source);
        return;
    }

    if (context->mode == ServerMode::BusyOnce && !context->busyNackSent)
    {
        context->busyNackSent = true;
        send_nack_or_report(
            *context,
            header.transactionId,
            weather::NackReason::Busy,
            source);
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

    if (request.intervalMilliseconds < 100u ||
        request.intervalMilliseconds > 60'000u)
    {
        send_nack_or_report(
            *context,
            header.transactionId,
            weather::NackReason::InvalidValue,
            source);
        return;
    }

    context->reportingIntervalMilliseconds = request.intervalMilliseconds;
    context->hasCompletedTransaction = true;
    context->completedTransactionId = header.transactionId;

    std::cout
        << "Transaction " << header.transactionId
        << ": reporting interval set to "
        << context->reportingIntervalMilliseconds << " ms"
        << ", idempotent=" << (isIdempotent(header) ? "yes" : "no")
        << "\n";

    if (context->mode == ServerMode::DropFirstAck &&
        !context->firstAckDropped)
    {
        context->firstAckDropped = true;
        std::cout << "Dropping ACK " << header.transactionId << "\n";
        return;
    }

    if (context->mode == ServerMode::Silent)
    {
        std::cout << "Response suppressed (silent mode)\n";
        return;
    }

    send_ack_or_report(*context, header.transactionId, source);
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
    context.mode = options.mode;

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
        << udp.local_port()
        << ", mode=" << mode_name(options.mode) << "\n";

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
