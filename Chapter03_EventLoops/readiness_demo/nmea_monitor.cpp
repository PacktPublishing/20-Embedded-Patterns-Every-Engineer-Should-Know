// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "EpollReactor.h"
#include "ImmutableByteView.h"
#include "SignalFdSource.h"
#include "TimerFdSource.h"
#include "UdpDatagramReceiver.h"
#include "UartLineReceiver.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace
{

struct Options
{
    std::optional<std::uint16_t> udpPort;
    std::optional<std::string> uartDevice;
    int uartBaud = 115200;
    int durationSeconds = 0;
    int pollTimeoutMs = -1;
    int statsEverySeconds = 5;
};

struct AppContext
{
    bool showSource = true;
    bool stopRequested = false;

    const pbook::UdpDatagramReceiver* udp = nullptr;
    const pbook::UartLineReceiver* uart = nullptr;
    const pbook::TimerFdSource* statsTimer = nullptr;
    const pbook::TimerFdSource* durationTimer = nullptr;
    const pbook::SignalFdSource* signalSource = nullptr;
};

void print_stats(const AppContext& context);

void print_usage(const char* argv0)
{
    std::cout
        << "Usage:\n"
        << "  " << argv0 << " --udp-port <port> [options]\n"
        << "  " << argv0 << " --uart <device> [--baud <rate>] [options]\n"
        << "  " << argv0 << " --udp-port <port> --uart <device> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --udp-port <port>       Listen for UDP datagrams on this local port\n"
        << "  --uart <device>         Read NMEA lines from a serial device or PTY\n"
        << "  --baud <rate>           UART baud rate. Default: 115200\n"
        << "  --duration <seconds>    Stop after this many seconds using timerfd. 0 runs until Ctrl-C. Default: 0\n"
        << "  --poll-ms <ms>          epoll timeout in milliseconds. -1 waits forever. Default: -1\n"
        << "  --stats-every <sec>     Print periodic stats using timerfd. 0 disables. Default: 5\n"
        << "  --no-source             Print only the received sentence text\n"
        << "  --help                  Print this help\n"
        << "\n"
        << "Examples:\n"
        << "  " << argv0 << " --udp-port 9000\n"
        << "  nmea_sim --udp 127.0.0.1:9000 --temp-hz 2 --pressure-hz 1\n"
        << "\n"
        << "  socat -d -d pty,raw,echo=0 pty,raw,echo=0\n"
        << "  " << argv0 << " --uart /dev/pts/2\n"
        << "  nmea_sim --uart /dev/pts/3 --temp-hz 2\n"
        << "\n"
        << "Signals:\n"
        << "  SIGINT and SIGTERM are handled through signalfd and dispatched by epoll.\n";
}

bool parse_u16(std::string_view text, std::uint16_t& out)
{
    std::string buffer{text};

    char* end = nullptr;
    const long value = std::strtol(buffer.c_str(), &end, 10);

    if (end == nullptr || *end != '\0' || value < 1 || value > 65535)
    {
        return false;
    }

    out = static_cast<std::uint16_t>(value);
    return true;
}

bool parse_int(std::string_view text, int& out)
{
    std::string buffer{text};

    char* end = nullptr;
    const long value = std::strtol(buffer.c_str(), &end, 10);

    if (end == nullptr || *end != '\0')
    {
        return false;
    }

    out = static_cast<int>(value);
    return true;
}

bool parse_options(int argc, char** argv, Options& options, AppContext& context)
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
        else if (arg == "--udp-port")
        {
            const char* value = require_value("--udp-port");
            if (value == nullptr)
            {
                return false;
            }

            std::uint16_t port = 0;
            if (!parse_u16(value, port))
            {
                std::cerr << "Invalid --udp-port value: " << value << "\n";
                return false;
            }

            options.udpPort = port;
        }
        else if (arg == "--uart")
        {
            const char* value = require_value("--uart");
            if (value == nullptr)
            {
                return false;
            }

            options.uartDevice = std::string{value};
        }
        else if (arg == "--baud")
        {
            const char* value = require_value("--baud");
            if (value == nullptr || !parse_int(value, options.uartBaud))
            {
                std::cerr << "Invalid --baud value\n";
                return false;
            }
        }
        else if (arg == "--duration")
        {
            const char* value = require_value("--duration");
            if (value == nullptr ||
                !parse_int(value, options.durationSeconds) ||
                options.durationSeconds < 0)
            {
                std::cerr << "Invalid --duration value\n";
                return false;
            }
        }
        else if (arg == "--poll-ms")
        {
            const char* value = require_value("--poll-ms");
            if (value == nullptr ||
                !parse_int(value, options.pollTimeoutMs) ||
                options.pollTimeoutMs < -1)
            {
                std::cerr << "Invalid --poll-ms value\n";
                return false;
            }
        }
        else if (arg == "--stats-every")
        {
            const char* value = require_value("--stats-every");
            if (value == nullptr ||
                !parse_int(value, options.statsEverySeconds) ||
                options.statsEverySeconds < 0)
            {
                std::cerr << "Invalid --stats-every value\n";
                return false;
            }
        }
        else if (arg == "--no-source")
        {
            context.showSource = false;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }

    if (!options.udpPort.has_value() && !options.uartDevice.has_value())
    {
        std::cerr << "Specify at least one input: --udp-port <port> or --uart <device>\n";
        return false;
    }

    return true;
}

void print_line(std::string_view prefix, std::string_view line, const AppContext& context)
{
    if (line.empty())
    {
        return;
    }

    if (context.showSource)
    {
        std::cout << prefix << " ";
    }

    std::cout.write(line.data(), static_cast<std::streamsize>(line.size()));
    std::cout << '\n';
}

void print_udp_datagram_lines(pbook::ImmutableByteView bytes,
                              const sockaddr_in& source,
                              const AppContext& context)
{
    char addr[INET_ADDRSTRLEN] = {};
    const char* printableAddr = ::inet_ntop(AF_INET, &source.sin_addr, addr, sizeof(addr));
    if (printableAddr == nullptr)
    {
        printableAddr = "?";
    }

    std::string prefix;
    if (context.showSource)
    {
        prefix = "[udp ";
        prefix += printableAddr;
        prefix += ":";
        prefix += std::to_string(ntohs(source.sin_port));
        prefix += "]";
    }

    const auto* chars = reinterpret_cast<const char*>(bytes.data());
    std::size_t start = 0;

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (chars[i] == '\n' || chars[i] == '\r')
        {
            if (i > start)
            {
                print_line(prefix, std::string_view{chars + start, i - start}, context);
            }

            start = i + 1;
        }
    }

    if (start < bytes.size())
    {
        print_line(prefix, std::string_view{chars + start, bytes.size() - start}, context);
    }
}

void on_udp_datagram(pbook::ImmutableByteView bytes,
                     const sockaddr_in& source,
                     void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    print_udp_datagram_lines(bytes, source, *context);
}

void on_uart_line(std::string_view line, void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    print_line("[uart]", line, *context);
}

void on_stats_timer(std::uint64_t expirations, void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    if (expirations > 1)
    {
        std::cout << "--- timer caught up: expirations=" << expirations << " ---\n";
    }

    print_stats(*context);
}

void on_duration_timer(std::uint64_t, void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    std::cout << "Duration timer expired. Stopping.\n";
    context->stopRequested = true;
}

void on_signal(int signo, void* userData) noexcept
{
    auto* context = static_cast<AppContext*>(userData);
    if (context == nullptr)
    {
        return;
    }

    std::cout << "Signal " << signo << " received through signalfd. Stopping.\n";
    context->stopRequested = true;
}

void print_port_stats(std::string_view name, const pbook::PortStats& stats)
{
    std::cout << name << ":"
              << " frames=" << stats.frames_received
              << " bytes=" << stats.bytes_received
              << " dropped=" << stats.frames_dropped
              << " read_err=" << stats.read_errors
              << " overflow=" << stats.overflow_errors
              << '\n';
}

void print_stats(const AppContext& context)
{
    std::cout << "--- stats ---\n";

    if (context.udp != nullptr)
    {
        print_port_stats("udp", context.udp->stats());
    }

    if (context.uart != nullptr)
    {
        print_port_stats("uart", context.uart->stats());
    }

    if (context.statsTimer != nullptr)
    {
        std::cout << "stats_timer:"
                  << " expirations=" << context.statsTimer->total_expirations()
                  << " read_errors=" << context.statsTimer->read_errors()
                  << '\n';
    }

    if (context.durationTimer != nullptr)
    {
        std::cout << "duration_timer:"
                  << " expirations=" << context.durationTimer->total_expirations()
                  << " read_errors=" << context.durationTimer->read_errors()
                  << '\n';
    }

    if (context.signalSource != nullptr)
    {
        std::cout << "signal_fd:"
                  << " signals=" << context.signalSource->signals_received()
                  << " read_errors=" << context.signalSource->read_errors()
                  << '\n';
    }

    std::cout << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    Options options{};
    AppContext context{};

    if (!parse_options(argc, argv, options, context))
    {
        print_usage(argv[0]);
        return 2;
    }

    pbook::EpollReactor reactor{16};
    if (!reactor.open())
    {
        std::cerr << "Failed to open epoll reactor\n";
        return 1;
    }

    std::optional<pbook::UdpDatagramReceiver> udp;
    std::optional<pbook::UartLineReceiver> uart;
    std::optional<pbook::TimerFdSource> statsTimer;
    std::optional<pbook::TimerFdSource> durationTimer;

    pbook::SignalFdSource signalSource{
                                       std::initializer_list<int>{SIGINT, SIGTERM},
                                       on_signal,
                                       &context};

    if (!signalSource.open())
    {
        std::cerr << "Failed to open signalfd source\n";
        return 1;
    }

    if (!reactor.add(signalSource))
    {
        std::cerr << "Failed to add signalfd source to epoll reactor\n";
        return 1;
    }

    context.signalSource = &signalSource;

    if (options.statsEverySeconds > 0)
    {
        statsTimer.emplace(on_stats_timer, &context);

        if (!statsTimer->open())
        {
            std::cerr << "Failed to open stats timerfd source\n";
            return 1;
        }

        if (!statsTimer->arm_periodic(std::chrono::seconds(options.statsEverySeconds)))
        {
            std::cerr << "Failed to arm stats timerfd source\n";
            return 1;
        }

        if (!reactor.add(*statsTimer))
        {
            std::cerr << "Failed to add stats timerfd source to epoll reactor\n";
            return 1;
        }

        context.statsTimer = &*statsTimer;
    }

    if (options.durationSeconds > 0)
    {
        durationTimer.emplace(on_duration_timer, &context);

        if (!durationTimer->open())
        {
            std::cerr << "Failed to open duration timerfd source\n";
            return 1;
        }

        if (!durationTimer->arm_oneshot(std::chrono::seconds(options.durationSeconds)))
        {
            std::cerr << "Failed to arm duration timerfd source\n";
            return 1;
        }

        if (!reactor.add(*durationTimer))
        {
            std::cerr << "Failed to add duration timerfd source to epoll reactor\n";
            return 1;
        }

        context.durationTimer = &*durationTimer;
    }

    if (options.udpPort.has_value())
    {
        udp.emplace(
            *options.udpPort,
            on_udp_datagram,
            &context);

        if (!udp->open())
        {
            std::cerr << "Failed to open UDP receiver on port " << *options.udpPort << "\n";
            return 1;
        }

        if (!reactor.add(*udp))
        {
            std::cerr << "Failed to add UDP receiver to epoll reactor\n";
            return 1;
        }

        context.udp = &*udp;

        std::cout << "Listening on UDP port " << udp->local_port() << "\n";
    }

    if (options.uartDevice.has_value())
    {
        uart.emplace(
            *options.uartDevice,
            options.uartBaud,
            on_uart_line,
            &context);

        if (!uart->open())
        {
            std::cerr << "Failed to open UART device " << *options.uartDevice << "\n";
            return 1;
        }

        if (!reactor.add(*uart))
        {
            std::cerr << "Failed to add UART receiver to epoll reactor\n";
            return 1;
        }

        context.uart = &*uart;

        std::cout << "Listening on UART " << *options.uartDevice
                  << " @ " << options.uartBaud << " baud\n";
    }

    if (options.statsEverySeconds > 0)
    {
        std::cout << "Stats timer every " << options.statsEverySeconds << " second(s)\n";
    }

    if (options.durationSeconds > 0)
    {
        std::cout << "Duration timer set for " << options.durationSeconds << " second(s)\n";
    }

    std::cout << "Waiting for NMEA input. Press Ctrl-C to stop.\n";

    while (!context.stopRequested)
    {
        const int dispatched = reactor.poll_once(options.pollTimeoutMs);
        if (dispatched < 0)
        {
            std::cerr << "epoll_wait failed\n";
            return 1;
        }
    }

    print_stats(context);
    return 0;
}
