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
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string_view>

using namespace std::chrono_literals;

namespace
{

volatile sig_atomic_t gStopRequested = 0;

void onSignal(int) noexcept
{
    gStopRequested = 1;
}

struct Endpoint
{
    sockaddr_in address{};
};

bool sameEndpoint(const Endpoint& a, const Endpoint& b) noexcept
{
    return a.address.sin_family == b.address.sin_family &&
           a.address.sin_port == b.address.sin_port &&
           a.address.sin_addr.s_addr == b.address.sin_addr.s_addr;
}

std::string endpointText(const Endpoint& endpoint)
{
    char address[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &endpoint.address.sin_addr, address, sizeof(address));

    char text[64]{};
    std::snprintf(
        text,
        sizeof(text),
        "%s:%u",
        address,
        static_cast<unsigned int>(ntohs(endpoint.address.sin_port)));
    return text;
}

struct Subscription
{
    bool active{};
    Endpoint endpoint{};
    ch15::ReportType report{ch15::ReportType::Temperature};
    std::chrono::milliseconds interval{};
    std::chrono::steady_clock::time_point nextDue{};
};

class WeatherDevice
{
public:
    explicit WeatherDevice(std::uint16_t port)
        : port_(port), started_(std::chrono::steady_clock::now())
    {
    }

    ~WeatherDevice()
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
        local.sin_port = htons(port_);

        if (::bind(socket_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0)
        {
            std::perror("bind");
            return false;
        }

        std::cout << "Weather Device listening on UDP port " << port_ << '\n';
        std::cout << "Maximum subscriptions: " << subscriptions_.size() << '\n';
        std::cout << "Allowed reporting interval: "
                  << MinInterval.count() << "-" << MaxInterval.count() << " ms\n";
        return true;
    }

    int run()
    {
        while (!gStopRequested)
        {
            pollfd descriptor{};
            descriptor.fd = socket_;
            descriptor.events = POLLIN;

            const int result = ::poll(&descriptor, 1, PollPeriod.count());
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                std::perror("poll");
                return 1;
            }

            if (result > 0 && (descriptor.revents & POLLIN) != 0)
            {
                receiveRequest();
            }

            sendDueReports();
        }

        std::cout << "Weather Device stopping\n";
        return 0;
    }

private:
    static constexpr std::chrono::milliseconds MinInterval{100};
    static constexpr std::chrono::milliseconds MaxInterval{60000};
    static constexpr std::chrono::milliseconds PollPeriod{25};

    void receiveRequest()
    {
        std::array<char, ch15::MaxSentenceSize> buffer{};
        Endpoint endpoint{};
        socklen_t endpointLength = sizeof(endpoint.address);

        const ssize_t received = ::recvfrom(
            socket_,
            buffer.data(),
            buffer.size() - 1,
            0,
            reinterpret_cast<sockaddr*>(&endpoint.address),
            &endpointLength);

        if (received <= 0)
        {
            return;
        }

        const std::string_view sentence(
            buffer.data(), static_cast<std::size_t>(received));

        ch15::ParsedSentence parsed{};
        const auto status = ch15::parseSentence(sentence, parsed);
        if (status != ch15::ParseStatus::Ok)
        {
            ++invalidMessages_;
            std::cout << "Rejected invalid NMEA request from "
                      << endpointText(endpoint) << ": "
                      << ch15::parseStatusName(status) << '\n';
            return;
        }

        std::cout << "RX " << endpointText(endpoint) << "  ";
        std::cout.write(sentence.data(), static_cast<std::streamsize>(sentence.size()));
        if (sentence.empty() || sentence.back() != '\n')
        {
            std::cout << '\n';
        }

        if (parsed.identifier == "WXSUB")
        {
            processSubscribe(endpoint, parsed);
        }
        else if (parsed.identifier == "WXUSB")
        {
            processUnsubscribe(endpoint, parsed);
        }
        else
        {
            ++invalidMessages_;
        }
    }

    void processSubscribe(
        const Endpoint& endpoint,
        const ch15::ParsedSentence& parsed)
    {
        if (parsed.fieldCount != 3)
        {
            return;
        }

        std::uint32_t transaction{};
        std::uint32_t intervalValue{};
        if (!ch15::parseUnsigned(parsed.fields[0], transaction) ||
            !ch15::parseUnsigned(parsed.fields[2], intervalValue))
        {
            return;
        }

        const auto report = ch15::parseReportType(parsed.fields[1]);
        if (!report.has_value())
        {
            sendAck(endpoint, transaction, "INVALID_PARAM");
            return;
        }

        const auto interval = std::chrono::milliseconds(intervalValue);
        if (interval < MinInterval || interval > MaxInterval)
        {
            sendAck(endpoint, transaction, "INVALID_PARAM");
            return;
        }

        if (!upsertSubscription(endpoint, *report, interval))
        {
            sendAck(endpoint, transaction, "BUSY");
            return;
        }

        sendAck(endpoint, transaction, "OK");
    }

    void processUnsubscribe(
        const Endpoint& endpoint,
        const ch15::ParsedSentence& parsed)
    {
        if (parsed.fieldCount != 2)
        {
            return;
        }

        std::uint32_t transaction{};
        if (!ch15::parseUnsigned(parsed.fields[0], transaction))
        {
            return;
        }

        const auto report = ch15::parseReportType(parsed.fields[1]);
        if (!report.has_value())
        {
            sendAck(endpoint, transaction, "INVALID_PARAM");
            return;
        }

        removeSubscription(endpoint, *report);
        sendAck(endpoint, transaction, "OK");
    }

    bool upsertSubscription(
        const Endpoint& endpoint,
        ch15::ReportType report,
        std::chrono::milliseconds interval)
    {
        const auto now = std::chrono::steady_clock::now();

        for (auto& subscription : subscriptions_)
        {
            if (subscription.active &&
                sameEndpoint(subscription.endpoint, endpoint) &&
                subscription.report == report)
            {
                subscription.interval = interval;
                subscription.nextDue = now + interval;
                std::cout << "Updated subscription " << endpointText(endpoint)
                          << " " << ch15::reportTypeCode(report)
                          << " every " << interval.count() << " ms\n";
                return true;
            }
        }

        for (auto& subscription : subscriptions_)
        {
            if (!subscription.active)
            {
                subscription.active = true;
                subscription.endpoint = endpoint;
                subscription.report = report;
                subscription.interval = interval;
                subscription.nextDue = now + interval;

                ++activeSubscriptions_;
                if (activeSubscriptions_ > subscriptionHighWater_)
                {
                    subscriptionHighWater_ = activeSubscriptions_;
                }

                std::cout << "Added subscription " << endpointText(endpoint)
                          << " " << ch15::reportTypeCode(report)
                          << " every " << interval.count() << " ms\n";
                return true;
            }
        }

        return false;
    }

    void removeSubscription(
        const Endpoint& endpoint,
        ch15::ReportType report)
    {
        for (auto& subscription : subscriptions_)
        {
            if (subscription.active &&
                sameEndpoint(subscription.endpoint, endpoint) &&
                subscription.report == report)
            {
                subscription.active = false;
                if (activeSubscriptions_ > 0)
                {
                    --activeSubscriptions_;
                }
                std::cout << "Removed subscription " << endpointText(endpoint)
                          << " " << ch15::reportTypeCode(report) << '\n';
                return;
            }
        }

        // Unsubscribe is intentionally idempotent. Removing a subscription that
        // is already absent is still considered successful.
        std::cout << "Subscription already absent " << endpointText(endpoint)
                  << " " << ch15::reportTypeCode(report) << '\n';
    }

    void sendAck(
        const Endpoint& endpoint,
        std::uint32_t transaction,
        std::string_view result)
    {
        char body[128]{};
        const int written = std::snprintf(
            body,
            sizeof(body),
            "WXACK,%u,%.*s",
            transaction,
            static_cast<int>(result.size()),
            result.data());

        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(body))
        {
            return;
        }

        ch15::SentenceBuffer sentence{};
        std::size_t length{};
        if (!ch15::buildSentence(
                std::string_view(body, static_cast<std::size_t>(written)),
                sentence,
                length))
        {
            return;
        }

        sendDatagram(endpoint, sentence.data(), length);
    }

    void sendDueReports()
    {
        const auto now = std::chrono::steady_clock::now();
        for (auto& subscription : subscriptions_)
        {
            if (!subscription.active || now < subscription.nextDue)
            {
                continue;
            }

            // Advance from the prior deadline so formatting/transmission time is
            // not added to every reporting interval.
            do
            {
                subscription.nextDue += subscription.interval;
            }
            while (subscription.nextDue <= now);

            sendReport(subscription);
        }
    }

    void sendReport(const Subscription& subscription)
    {
        const auto sequence = nextSequence_++;
        char timeText[16]{};
        formatUtcTime(timeText, sizeof(timeText));

        char body[192]{};
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started_).count();

        int written{};
        switch (subscription.report)
        {
        case ch15::ReportType::Status:
        {
            const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - started_).count();
            written = std::snprintf(
                body, sizeof(body),
                "WXSTS,%u,%s,RUNNING,%lld",
                sequence, timeText, static_cast<long long>(uptime));
            break;
        }

        case ch15::ReportType::Health:
            written = std::snprintf(
                body, sizeof(body),
                "WXHLT,%u,%s,OK,OK,OK,OK",
                sequence, timeText);
            break;

        case ch15::ReportType::Temperature:
        {
            const double temperature = 22.5 + 1.5 * std::sin(elapsed / 8.0);
            written = std::snprintf(
                body, sizeof(body),
                "WXTMP,%u,%s,%.1f,C",
                sequence, timeText, temperature);
            break;
        }

        case ch15::ReportType::Pressure:
        {
            const double pressure = 1008.0 + 2.0 * std::sin(elapsed / 17.0);
            written = std::snprintf(
                body, sizeof(body),
                "WXPRS,%u,%s,%.1f,HPA",
                sequence, timeText, pressure);
            break;
        }

        case ch15::ReportType::Wind:
        {
            const int direction = static_cast<int>(std::fmod(220.0 + elapsed * 2.0, 360.0));
            const double speed = 8.0 + 2.0 * std::sin(elapsed / 6.0);
            written = std::snprintf(
                body, sizeof(body),
                "WXWND,%u,%s,%d,%.1f,MPH",
                sequence, timeText, direction, speed);
            break;
        }

        case ch15::ReportType::Diagnostic:
            written = std::snprintf(
                body, sizeof(body),
                "WXDIA,%u,%s,%u,%zu,%u",
                sequence,
                timeText,
                droppedReports_,
                subscriptionHighWater_,
                retryCount_);
            break;
        }

        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(body))
        {
            ++droppedReports_;
            return;
        }

        ch15::SentenceBuffer sentence{};
        std::size_t length{};
        if (!ch15::buildSentence(
                std::string_view(body, static_cast<std::size_t>(written)),
                sentence,
                length))
        {
            ++droppedReports_;
            return;
        }

        if (!sendDatagram(subscription.endpoint, sentence.data(), length))
        {
            ++droppedReports_;
        }
    }

    bool sendDatagram(
        const Endpoint& endpoint,
        const char* data,
        std::size_t length)
    {
        const ssize_t sent = ::sendto(
            socket_,
            data,
            length,
            0,
            reinterpret_cast<const sockaddr*>(&endpoint.address),
            sizeof(endpoint.address));

        return sent == static_cast<ssize_t>(length);
    }

    static void formatUtcTime(char* output, std::size_t capacity)
    {
        const auto now = std::chrono::system_clock::now();
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        const std::time_t value = std::chrono::system_clock::to_time_t(now);

        std::tm utc{};
        ::gmtime_r(&value, &utc);

        std::snprintf(
            output,
            capacity,
            "%02d%02d%02d.%03lld",
            utc.tm_hour,
            utc.tm_min,
            utc.tm_sec,
            static_cast<long long>(milliseconds.count()));
    }

    std::uint16_t port_{};
    int socket_{-1};
    std::array<Subscription, 16> subscriptions_{};
    std::size_t activeSubscriptions_{};
    std::size_t subscriptionHighWater_{};
    std::uint32_t nextSequence_{1};
    std::uint32_t droppedReports_{};
    std::uint32_t retryCount_{};
    std::uint32_t invalidMessages_{};
    std::chrono::steady_clock::time_point started_{};
};

std::uint16_t parsePort(int argc, char** argv)
{
    std::uint16_t port = 9500;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg(argv[i]);
        if (arg == "--port" && i + 1 < argc)
        {
            std::uint32_t value{};
            if (ch15::parseUnsigned(argv[++i], value) && value <= 65535)
            {
                port = static_cast<std::uint16_t>(value);
            }
        }
        else if (arg == "--help")
        {
            std::cout << "Usage: ch15_weather_device [--port <udp-port>]\n";
            std::exit(0);
        }
    }
    return port;
}

} // namespace

int main(int argc, char** argv)
{
    ::signal(SIGINT, onSignal);
    ::signal(SIGTERM, onSignal);

    WeatherDevice device(parsePort(argc, argv));
    if (!device.open())
    {
        return 1;
    }

    return device.run();
}
