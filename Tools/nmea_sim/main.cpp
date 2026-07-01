#include "NmeaSimApp.h"
#include "Options.h"
#include "TransmitEndpointFactory.h"
#include "UdpEndpointParse.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using nmea_sim::Options;

static void usage()
{
    std::cout <<
R"(nmea_sim - generate synthetic NMEA-style weather data

Usage:
  nmea_sim [output] [streams] [options]

Output, choose exactly one:
  --stdout                      write NMEA sentences to stdout
  --file <path>                 write NMEA sentences to a file
  --uart <device>               write to a serial device or PTY, e.g. /dev/pts/7
  --udp <host:port>             send UDP datagrams, e.g. 127.0.0.1:9000

Streams, Hz; 0 disables a stream:
  --temp-hz <value>             temperature stream rate
  --pressure-hz <value>         pressure stream rate
  --humidity-hz <value>         humidity stream rate
  --position-hz <value>         position stream rate
  --wind-hz <value>             wind stream rate

Options:
  --baud <rate>                 UART baud rate, default 115200
  --gen <kind>                  constant|sine|random-walk, default random-walk
  --seed <n>                    deterministic PRNG seed, default 1
  --duration <seconds>          0 runs forever, default 0
  --log-every <seconds>         status log period, default 5
  --bad-checksum-percent <p>    corrupt checksums for p percent of messages
  --truncated-percent <p>       truncate p percent of messages
  -h, --help                    print this help

Examples:
  nmea_sim --stdout --temp-hz 1 --duration 5
  nmea_sim --file capture.nmea --temp-hz 2 --position-hz 1 --duration 30
  nmea_sim --udp 127.0.0.1:9000 --temp-hz 2 --pressure-hz 1
  nmea_sim --uart /dev/pts/7 --temp-hz 1 --bad-checksum-percent 2
)";
}

static bool parse_double(const char* s, double& out)
{
    char* end = nullptr;
    out = std::strtod(s, &end);
    return end != nullptr && *end == '\0';
}

static bool parse_u32(const char* s, std::uint32_t& out)
{
    char* end = nullptr;
    const unsigned long v = std::strtoul(s, &end, 10);
    if (end == nullptr || *end != '\0') return false;
    out = static_cast<std::uint32_t>(v);
    return true;
}

static bool parse_gen(std::string_view s, nmea_sim::GeneratorKind& out)
{
    if (s == "constant") { out = nmea_sim::GeneratorKind::Constant; return true; }
    if (s == "sine") { out = nmea_sim::GeneratorKind::Sine; return true; }
    if (s == "random-walk") { out = nmea_sim::GeneratorKind::RandomWalk; return true; }
    return false;
}

static bool need_value(int i, int argc, const char* opt)
{
    if (i + 1 < argc) return true;
    std::cerr << opt << " requires a value\n";
    return false;
}

static bool parse_args(int argc, char** argv, Options& opt)
{
    auto consume_value = [&](int& i, std::string_view option) -> const char*
    {
        if (!need_value(i, argc, option.data()))
        {
            return nullptr;
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view a(argv[i]);

        if (a == "-h" || a == "--help")
        {
            usage();
            std::exit(0);
        }
        else if (a == "--stdout")
        {
            opt.use_stdout = true;
        }
        else if (a == "--file")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            opt.file = value;
        }
        else if (a == "--uart")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            opt.uart_device = value;
        }
        else if (a == "--udp")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;

            nmea_sim::UdpEndpoint ep{};
            if (!nmea_sim::parse_udp_endpoint(value, ep)) return false;
            opt.udp = ep;
        }
        else if (a == "--baud")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            opt.uart_baud = std::atoi(value);
        }
        else if (a == "--temp-hz")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.temp_hz)) return false;
        }
        else if (a == "--pressure-hz")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.pressure_hz)) return false;
        }
        else if (a == "--humidity-hz")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.humidity_hz)) return false;
        }
        else if (a == "--position-hz")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.position_hz)) return false;
        }
        else if (a == "--wind-hz")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.wind_hz)) return false;
        }
        else if (a == "--gen")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_gen(value, opt.gen)) return false;
        }
        else if (a == "--seed")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_u32(value, opt.seed)) return false;
        }
        else if (a == "--duration")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.duration_sec)) return false;
        }
        else if (a == "--log-every")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.log_every_sec)) return false;
        }
        else if (a == "--bad-checksum-percent")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.bad_checksum_percent)) return false;
        }
        else if (a == "--truncated-percent")
        {
            const char* value = consume_value(i, a);
            if (!value) return false;
            if (!parse_double(value, opt.truncated_percent)) return false;
        }
        else
        {
            std::cerr << "Unknown or invalid option: " << a << "\n";
            return false;
        }
    }

    return true;
}

static bool validate(const Options& opt)
{
    const int outputs = (opt.uart_device ? 1 : 0) + (opt.udp ? 1 : 0) +
                        (opt.file ? 1 : 0) + (opt.use_stdout ? 1 : 0);
    if (outputs != 1)
    {
        std::cerr << "Specify exactly one output: --stdout, --file, --uart, or --udp\n";
        return false;
    }
    if (opt.temp_hz < 0 || opt.pressure_hz < 0 || opt.humidity_hz < 0 ||
        opt.position_hz < 0 || opt.wind_hz < 0)
    {
        std::cerr << "Stream rates must be >= 0\n";
        return false;
    }
    if (opt.temp_hz == 0 && opt.pressure_hz == 0 && opt.humidity_hz == 0 &&
        opt.position_hz == 0 && opt.wind_hz == 0)
    {
        std::cerr << "Enable at least one stream rate\n";
        return false;
    }
    if (opt.duration_sec < 0 || opt.log_every_sec <= 0 ||
        opt.bad_checksum_percent < 0 || opt.truncated_percent < 0 ||
        opt.bad_checksum_percent + opt.truncated_percent > 100)
    {
        std::cerr << "Invalid timing or fault-injection option\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    Options opt{};
    if (!parse_args(argc, argv, opt) || !validate(opt))
    {
        usage();
        return 2;
    }

    auto sink = nmea_sim::create_transmitter(opt);
    if (!sink)
    {
        std::cerr << "Failed to create output endpoint\n";
        return 2;
    }

    nmea_sim::NmeaSimApp app(opt, std::move(sink));
    return app.run();
}
