// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
#include "EpollReactor.h"
#include "MessageFrame.h"
#include "MessageHeader.h"
#include "SignalFdSource.h"
#include "TimerFdSource.h"
#include "UdpDatagramReceiver.h"
#include "WeatherControlProtocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <chrono>
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
    std::string serverAddress{"127.0.0.1"};
    std::uint16_t serverPort{9100};
    std::uint32_t intervalMilliseconds{5000};
    std::uint32_t transactionId{42};
    std::uint32_t responseTimeoutMilliseconds{500};
    std::uint32_t retryDelayMilliseconds{250};
    std::uint32_t maxAttempts{3};
};

struct RetryPolicy
{
    std::chrono::milliseconds responseTimeout{500};
    std::chrono::milliseconds retryDelay{250};
    std::uint32_t maxAttempts{3};
};

enum class TransactionState
{
    Idle,
    WaitingForResponse,
    WaitingToRetry,
    Succeeded,
    Failed
};

struct Transaction
{
    std::uint32_t id{};
    std::uint32_t attempts{};
    TransactionState state{TransactionState::Idle};
};

struct AppContext
{
    bool stopRequested{false};
    bool succeeded{false};
    bool serviceAvailable{true};

    Options options{};
    RetryPolicy policy{};
    Transaction transaction{};
    sockaddr_in destination{};

    pbook::UdpDatagramReceiver* udp{nullptr};
    pbook::TimerFdSource* timer{nullptr};
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

bool parse_u32(std::string_view text, std::uint32_t& out, bool allowZero = false)
{
    std::string buffer{text};
    char* end = nullptr;
    const unsigned long long value = std::strtoull(buffer.c_str(), &end, 10);

    if (end == nullptr || *end != '\0' ||
        (!allowZero && value == 0) || value > 0xFFFF'FFFFull)
    {
        return false;
    }

    out = static_cast<std::uint32_t>(value);
    return true;
}

void print_usage(const char* argv0)
{
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --server <IPv4>          Server address. Default: 127.0.0.1\n"
        << "  --port <port>            Server UDP port. Default: 9100\n"
        << "  --interval-ms <ms>       Reporting interval to request. Default: 5000\n"
        << "  --transaction <id>       Nonzero transaction ID. Default: 42\n"
        << "  --timeout-ms <ms>        Response timeout. Default: 500\n"
        << "  --retry-delay-ms <ms>    Delay before a retry. Default: 250\n"
        << "  --max-attempts <count>   Total attempts including the first. Default: 3\n"
        << "  --help                   Print this help\n";
}

bool parse_options(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};

        auto require_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc)
            {
                std::cerr << name << " requires a value\n";
                return nullptr;
            }

            return argv[++i];
        };

        if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            std::exit(0);
        }
        else if (arg == "--server")
        {
            const char* value = require_value("--server");
            if (value == nullptr)
            {
                return false;
            }
            options.serverAddress = value;
        }
        else if (arg == "--port")
        {
            const char* value = require_value("--port");
            if (value == nullptr || !parse_u16(value, options.serverPort))
            {
                std::cerr << "Invalid --port value\n";
                return false;
            }
        }
        else if (arg == "--interval-ms")
        {
            const char* value = require_value("--interval-ms");
            if (value == nullptr || !parse_u32(value, options.intervalMilliseconds))
            {
                std::cerr << "Invalid --interval-ms value\n";
                return false;
            }
        }
        else if (arg == "--transaction")
        {
            const char* value = require_value("--transaction");
            if (value == nullptr || !parse_u32(value, options.transactionId))
            {
                std::cerr << "Invalid --transaction value\n";
                return false;
            }
        }
        else if (arg == "--timeout-ms")
        {
            const char* value = require_value("--timeout-ms");
            if (value == nullptr || !parse_u32(value, options.responseTimeoutMilliseconds))
            {
                std::cerr << "Invalid --timeout-ms value\n";
                return false;
            }
        }
        else if (arg == "--retry-delay-ms")
        {
            const char* value = require_value("--retry-delay-ms");
            if (value == nullptr || !parse_u32(value, options.retryDelayMilliseconds))
            {
                std::cerr << "Invalid --retry-delay-ms value\n";
                return false;
            }
        }
        else if (arg == "--max-attempts")
        {
            const char* value = require_value("--max-attempts");
            if (value == nullptr || !parse_u32(value, options.maxAttempts))
            {
                std::cerr << "Invalid --max-attempts value\n";
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

bool make_destination(const Options& options, sockaddr_in& destination) noexcept
{
    destination = {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(options.serverPort);

    return ::inet_pton(
               AF_INET,
               options.serverAddress.c_str(),
               &destination.sin_addr) == 1;
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

bool send_request_frame(AppContext& context) noexcept
{
    if (context.udp == nullptr)
    {
        return false;
    }

    std::array<std::byte, 32> payloadStorage{};
    pbook::BinaryWriteStream payloadWriter(
        pbook::MutableByteView{
            payloadStorage.data(),
            payloadStorage.size()},
        Endianness::Little);

    const weather::SetReportingIntervalRequest request{
        .intervalMilliseconds = context.options.intervalMilliseconds};

    weather::writeSetReportingIntervalRequest(payloadWriter, request);
    if (!payloadWriter.ok())
    {
        return false;
    }

    const pbook::ImmutableByteView payload{
        payloadStorage.data(),
        payloadWriter.bytesWritten()};

    MessageHeaderV1 header{};
    header.payloadEndian = 0u;
    header.serviceId = weather::WeatherControlServiceId;
    header.messageType = static_cast<std::uint16_t>(
        weather::ControlMessageType::SetReportingInterval);
    header.transactionId = context.transaction.id;
    header.flags = MessageFlagIdempotent;

    std::array<std::byte, 256> frameStorage{};
    std::size_t frameSize = 0u;

    const auto frameStatus = writeFrameV1(
        pbook::MutableByteView{
            frameStorage.data(),
            frameStorage.size()},
        header,
        payload,
        frameSize);

    if (frameStatus != MessageFrameStatus::Ok)
    {
        return false;
    }

    return send_datagram(
        context.udp->fd(),
        pbook::ImmutableByteView{frameStorage.data(), frameSize},
        context.destination);
}

void fail_transaction(AppContext& context, const char* reason) noexcept
{
    if (context.timer != nullptr)
    {
        context.timer->disarm();
    }

    context.transaction.state = TransactionState::Failed;
    context.succeeded = false;
    context.stopRequested = true;
    std::cout << "Transaction failed: " << reason << "\n";
}

void exhaust_retry_budget(AppContext& context) noexcept
{
    std::cout << "Retry budget exhausted\n";
    context.serviceAvailable = false;
    context.transaction.state = TransactionState::Failed;
    context.succeeded = false;
    context.stopRequested = true;
    std::cout << "Recovery: remote service marked unavailable\n";
}

bool arm_retry_delay(AppContext& context) noexcept
{
    if (context.timer == nullptr)
    {
        return false;
    }

    context.transaction.state = TransactionState::WaitingToRetry;
    std::cout
        << "Retrying after " << context.policy.retryDelay.count()
        << " ms\n";

    return context.timer->arm_oneshot(context.policy.retryDelay);
}

void schedule_retry(AppContext& context) noexcept
{
    if (context.transaction.attempts >= context.policy.maxAttempts)
    {
        exhaust_retry_budget(context);
        return;
    }

    if (!arm_retry_delay(context))
    {
        fail_transaction(context, "could not arm retry timer");
    }
}

bool send_attempt(AppContext& context) noexcept
{
    ++context.transaction.attempts;

    std::cout
        << "Sending transaction " << context.transaction.id
        << ", attempt " << context.transaction.attempts
        << "/" << context.policy.maxAttempts
        << ": set reporting interval to "
        << context.options.intervalMilliseconds << " ms\n";

    if (!send_request_frame(context))
    {
        std::cerr << "Failed to send request";
        if (errno != 0)
        {
            std::cerr << ": errno=" << errno;
        }
        std::cerr << "\n";
        fail_transaction(context, "local transmission failure");
        return false;
    }

    if (context.timer == nullptr ||
        !context.timer->arm_oneshot(context.policy.responseTimeout))
    {
        fail_transaction(context, "could not arm response timer");
        return false;
    }

    context.transaction.state = TransactionState::WaitingForResponse;
    return true;
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

void on_transaction_timer(std::uint64_t, void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    if (context->transaction.state == TransactionState::WaitingForResponse)
    {
        std::cout
            << "Response timeout for transaction "
            << context->transaction.id << "\n";
        schedule_retry(*context);
        return;
    }

    if (context->transaction.state == TransactionState::WaitingToRetry)
    {
        send_attempt(*context);
    }
}

void on_response(pbook::ImmutableByteView bytes,
                 const sockaddr_in&,
                 void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    if (context->transaction.state != TransactionState::WaitingForResponse &&
        context->transaction.state != TransactionState::WaitingToRetry)
    {
        return;
    }

    MessageHeaderV1 header{};
    pbook::ImmutableByteView payload;

    const auto status = readFrameV1(bytes, header, payload);
    if (status != MessageFrameStatus::Ok)
    {
        std::cerr << "Ignoring invalid BDS response\n";
        return;
    }

    if (header.serviceId != weather::WeatherControlServiceId)
    {
        std::cerr << "Ignoring response for another service\n";
        return;
    }

    if (header.transactionId != context->transaction.id)
    {
        std::cerr
            << "Ignoring response for transaction " << header.transactionId
            << "; waiting for " << context->transaction.id << "\n";
        return;
    }

    const auto messageType =
        static_cast<weather::ControlMessageType>(header.messageType);

    if (messageType == weather::ControlMessageType::Ack)
    {
        if (!payload.empty())
        {
            std::cerr << "Ignoring malformed ACK with a payload\n";
            return;
        }

        if (context->timer != nullptr)
        {
            context->timer->disarm();
        }

        std::cout << "ACK " << header.transactionId << " received\n";
        std::cout << "Transaction succeeded\n";
        context->transaction.state = TransactionState::Succeeded;
        context->succeeded = true;
        context->stopRequested = true;
        return;
    }

    if (messageType == weather::ControlMessageType::Nack)
    {
        weather::NackResponse nack{};
        pbook::BinaryReadStream reader(
            payload,
            payloadEndianFromHeader(header.payloadEndian));

        weather::readNackResponse(reader, nack);

        if (!reader.ok() || reader.remaining() != 0u ||
            !weather::isKnownNackReason(nack.reason))
        {
            std::cerr << "Ignoring malformed NACK\n";
            return;
        }

        if (context->timer != nullptr)
        {
            context->timer->disarm();
        }

        std::cout
            << "NACK " << header.transactionId
            << " received: " << nack_reason_name(nack.reason) << "\n";

        if (nack.reason == weather::NackReason::Busy)
        {
            std::cout << "NACK is retryable\n";
            schedule_retry(*context);
        }
        else
        {
            std::cout << "NACK is not retryable\n";
            fail_transaction(*context, nack_reason_name(nack.reason));
        }
        return;
    }

    std::cerr << "Ignoring unexpected response message type\n";
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
    context.options = options;
    context.policy.responseTimeout =
        std::chrono::milliseconds{options.responseTimeoutMilliseconds};
    context.policy.retryDelay =
        std::chrono::milliseconds{options.retryDelayMilliseconds};
    context.policy.maxAttempts = options.maxAttempts;
    context.transaction.id = options.transactionId;

    if (!make_destination(options, context.destination))
    {
        std::cerr << "Invalid IPv4 server address: "
                  << options.serverAddress << "\n";
        return 1;
    }

    pbook::EpollReactor reactor{8};
    if (!reactor.open())
    {
        std::cerr << "Failed to open epoll reactor\n";
        return 1;
    }

    // Bind to port 0 so the kernel chooses an ephemeral client port. The same
    // socket is used to transmit the request and receive the response.
    pbook::UdpDatagramReceiver udp{0, on_response, &context};
    if (!udp.open())
    {
        std::cerr << "Failed to open UDP client socket\n";
        return 1;
    }

    context.udp = &udp;

    if (!reactor.add(udp))
    {
        std::cerr << "Failed to add UDP socket to epoll reactor\n";
        return 1;
    }

    pbook::TimerFdSource transactionTimer{on_transaction_timer, &context};
    if (!transactionTimer.open() || !reactor.add(transactionTimer))
    {
        std::cerr << "Failed to configure transaction timer\n";
        return 1;
    }

    context.timer = &transactionTimer;

    pbook::SignalFdSource signalSource{
        std::initializer_list<int>{SIGINT, SIGTERM},
        on_signal,
        &context};

    if (!signalSource.open() || !reactor.add(signalSource))
    {
        std::cerr << "Failed to configure signal handling\n";
        return 1;
    }

    if (!send_attempt(context))
    {
        return 1;
    }

    while (!context.stopRequested)
    {
        if (reactor.poll_once(-1) < 0)
        {
            std::cerr << "epoll wait failed\n";
            return 1;
        }
    }

    return context.succeeded ? 0 : 2;
}
