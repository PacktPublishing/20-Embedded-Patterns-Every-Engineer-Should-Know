// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "WeatherMonitoringProtocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

using namespace std::chrono_literals;

namespace
{

volatile sig_atomic_t gStopRequested = 0;

void onSignal(int) noexcept
{
    gStopRequested = 1;
}

struct Options
{
    std::string host{"127.0.0.1"};
    std::uint16_t port{9500};
    bool demo{false};
};

class RemoteMonitor
{
public:
    explicit RemoteMonitor(const Options& options)
        : options_(options)
    {
    }

    ~RemoteMonitor()
    {
        if (socket_ >= 0)
        {
            ::close(socket_);
        }
    }

    bool open()
    {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0)
        {
            std::perror("socket");
            return false;
        }

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(0);

        if (::bind(socket_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0)
        {
            std::perror("bind");
            return false;
        }

        server_.sin_family = AF_INET;
        server_.sin_port = htons(options_.port);
        if (::inet_pton(AF_INET, options_.host.c_str(), &server_.sin_addr) != 1)
        {
            std::cerr << "Invalid IPv4 address: " << options_.host << '\n';
            return false;
        }

        sockaddr_in assigned{};
        socklen_t assignedLength = sizeof(assigned);
        if (::getsockname(
                socket_,
                reinterpret_cast<sockaddr*>(&assigned),
                &assignedLength) == 0)
        {
            std::cout << "Remote Monitor UDP port " << ntohs(assigned.sin_port) << '\n';
        }

        std::cout << "Weather Device endpoint "
                  << options_.host << ':' << options_.port << '\n';
        return true;
    }

    int run()
    {
        if (options_.demo)
        {
            return runDemo();
        }

        std::cout << "No subscriptions requested. Use --demo for the chapter lab.\n";
        return receiveLoop();
    }

private:
    static constexpr std::chrono::milliseconds AckTimeout{700};
    static constexpr int MaxAttempts = 3;

    int runDemo()
    {
        std::cout << "\nSubscribing to temperature, pressure, health, and diagnostics...\n";

        if (!subscribe(201, "TMP", 1000) ||
            !subscribe(202, "PRS", 2000) ||
            !subscribe(203, "HLT", 5000) ||
            !subscribe(204, "DIA", 5000))
        {
            return 1;
        }

        const auto unsubscribeAt = std::chrono::steady_clock::now() + 10s;
        bool temperatureCancelled = false;

        std::cout << "\nReceiving reports. Temperature will be unsubscribed after 10 seconds.\n\n";

        while (!gStopRequested)
        {
            if (!temperatureCancelled &&
                std::chrono::steady_clock::now() >= unsubscribeAt)
            {
                std::cout << "\nUnsubscribing from temperature reports...\n";
                if (!unsubscribe(205, "TMP"))
                {
                    return 1;
                }
                temperatureCancelled = true;
                std::cout << "Temperature reports stopped; other subscriptions remain active.\n\n";
            }

            receiveOne(250ms, std::nullopt);
        }

        std::cout << "Remote Monitor stopping\n";
        return 0;
    }

    int receiveLoop()
    {
        while (!gStopRequested)
        {
            receiveOne(500ms, std::nullopt);
        }
        return 0;
    }

    bool subscribe(
        std::uint32_t transaction,
        std::string_view report,
        std::uint32_t interval)
    {
        char body[128]{};
        const int written = std::snprintf(
            body,
            sizeof(body),
            "WXSUB,%u,%.*s,%u",
            transaction,
            static_cast<int>(report.size()),
            report.data(),
            interval);

        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(body))
        {
            return false;
        }

        return transact(
            transaction,
            std::string_view(body, static_cast<std::size_t>(written)));
    }

    bool unsubscribe(
        std::uint32_t transaction,
        std::string_view report)
    {
        char body[128]{};
        const int written = std::snprintf(
            body,
            sizeof(body),
            "WXUSB,%u,%.*s",
            transaction,
            static_cast<int>(report.size()),
            report.data());

        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(body))
        {
            return false;
        }

        return transact(
            transaction,
            std::string_view(body, static_cast<std::size_t>(written)));
    }

    bool transact(std::uint32_t transaction, std::string_view body)
    {
        ch15::SentenceBuffer sentence{};
        std::size_t length{};
        if (!ch15::buildSentence(body, sentence, length))
        {
            return false;
        }

        for (int attempt = 1; attempt <= MaxAttempts && !gStopRequested; ++attempt)
        {
            std::cout << "TX attempt " << attempt << '/' << MaxAttempts << "  ";
            std::cout.write(sentence.data(), static_cast<std::streamsize>(length));

            const ssize_t sent = ::sendto(
                socket_,
                sentence.data(),
                length,
                0,
                reinterpret_cast<const sockaddr*>(&server_),
                sizeof(server_));

            if (sent != static_cast<ssize_t>(length))
            {
                std::perror("sendto");
                continue;
            }

            const auto deadline = std::chrono::steady_clock::now() + AckTimeout;
            while (!gStopRequested && std::chrono::steady_clock::now() < deadline)
            {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now());

                const auto result = receiveOne(remaining, transaction);
                if (result == AckResult::MatchedOk)
                {
                    return true;
                }
                if (result == AckResult::MatchedFailure)
                {
                    return false;
                }
            }

            std::cout << "ACK timeout for transaction " << transaction << '\n';
        }

        std::cerr << "Transaction " << transaction << " failed after "
                  << MaxAttempts << " attempts\n";
        return false;
    }

    enum class AckResult
    {
        None,
        MatchedOk,
        MatchedFailure
    };

    AckResult receiveOne(
        std::chrono::milliseconds timeout,
        std::optional<std::uint32_t> expectedTransaction)
    {
        pollfd descriptor{};
        descriptor.fd = socket_;
        descriptor.events = POLLIN;

        int timeoutMs = static_cast<int>(timeout.count());
        if (timeoutMs < 0)
        {
            timeoutMs = 0;
        }

        const int result = ::poll(&descriptor, 1, timeoutMs);
        if (result <= 0 || (descriptor.revents & POLLIN) == 0)
        {
            return AckResult::None;
        }

        std::array<char, ch15::MaxSentenceSize> buffer{};
        sockaddr_in source{};
        socklen_t sourceLength = sizeof(source);
        const ssize_t received = ::recvfrom(
            socket_,
            buffer.data(),
            buffer.size() - 1,
            0,
            reinterpret_cast<sockaddr*>(&source),
            &sourceLength);

        if (received <= 0)
        {
            return AckResult::None;
        }

        const std::string_view sentence(
            buffer.data(), static_cast<std::size_t>(received));

        ch15::ParsedSentence parsed{};
        const auto status = ch15::parseSentence(sentence, parsed);
        if (status != ch15::ParseStatus::Ok)
        {
            std::cout << "RX invalid NMEA: " << ch15::parseStatusName(status) << '\n';
            return AckResult::None;
        }

        std::cout << "RX  ";
        std::cout.write(sentence.data(), static_cast<std::streamsize>(sentence.size()));
        if (sentence.empty() || sentence.back() != '\n')
        {
            std::cout << '\n';
        }

        if (parsed.identifier == "WXACK")
        {
            if (parsed.fieldCount != 2)
            {
                return AckResult::None;
            }

            std::uint32_t transaction{};
            if (!ch15::parseUnsigned(parsed.fields[0], transaction))
            {
                return AckResult::None;
            }

            if (!expectedTransaction.has_value() || transaction != *expectedTransaction)
            {
                return AckResult::None;
            }

            return parsed.fields[1] == "OK"
                ? AckResult::MatchedOk
                : AckResult::MatchedFailure;
        }

        printDecodedReport(parsed);
        return AckResult::None;
    }

    static void printDecodedReport(const ch15::ParsedSentence& parsed)
    {
        if (parsed.identifier == "WXTMP" && parsed.fieldCount == 4)
        {
            std::cout << "    temperature=" << parsed.fields[2]
                      << ' ' << parsed.fields[3] << '\n';
        }
        else if (parsed.identifier == "WXPRS" && parsed.fieldCount == 4)
        {
            std::cout << "    pressure=" << parsed.fields[2]
                      << ' ' << parsed.fields[3] << '\n';
        }
        else if (parsed.identifier == "WXHLT" && parsed.fieldCount == 6)
        {
            std::cout << "    overall=" << parsed.fields[2]
                      << " temperature=" << parsed.fields[3]
                      << " pressure=" << parsed.fields[4]
                      << " wind=" << parsed.fields[5] << '\n';
        }
        else if (parsed.identifier == "WXDIA" && parsed.fieldCount == 5)
        {
            std::cout << "    dropped=" << parsed.fields[2]
                      << " subscription_high_water=" << parsed.fields[3]
                      << " retries=" << parsed.fields[4] << '\n';
        }
        else if (parsed.identifier == "WXSTS" && parsed.fieldCount == 4)
        {
            std::cout << "    state=" << parsed.fields[2]
                      << " uptime=" << parsed.fields[3] << " s\n";
        }
        else if (parsed.identifier == "WXWND" && parsed.fieldCount == 5)
        {
            std::cout << "    direction=" << parsed.fields[2]
                      << " speed=" << parsed.fields[3]
                      << ' ' << parsed.fields[4] << '\n';
        }
    }

    Options options_{};
    int socket_{-1};
    sockaddr_in server_{};
};

Options parseOptions(int argc, char** argv)
{
    Options options{};

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg(argv[i]);
        if (arg == "--host" && i + 1 < argc)
        {
            options.host = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            std::uint32_t value{};
            if (ch15::parseUnsigned(argv[++i], value) && value <= 65535)
            {
                options.port = static_cast<std::uint16_t>(value);
            }
        }
        else if (arg == "--demo")
        {
            options.demo = true;
        }
        else if (arg == "--help")
        {
            std::cout
                << "Usage: ch15_remote_monitor [options]\n"
                << "  --host <IPv4>   Weather Device address. Default: 127.0.0.1\n"
                << "  --port <port>   Weather Device UDP port. Default: 9500\n"
                << "  --demo          Run the Chapter 15 subscription demonstration\n";
            std::exit(0);
        }
    }

    return options;
}

} // namespace

int main(int argc, char** argv)
{
    ::signal(SIGINT, onSignal);
    ::signal(SIGTERM, onSignal);

    const auto options = parseOptions(argc, argv);
    RemoteMonitor monitor(options);
    if (!monitor.open())
    {
        return 1;
    }

    return monitor.run();
}
