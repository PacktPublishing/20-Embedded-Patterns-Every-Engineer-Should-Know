#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace nmea_sim
{
enum class GeneratorKind { Constant, Sine, RandomWalk };

struct UdpEndpoint
{
    std::string host;
    std::uint16_t port = 0;
};

struct Options
{
    std::optional<std::string> uart_device;
    int uart_baud = 115200;
    std::optional<UdpEndpoint> udp;
    std::optional<std::string> file;
    bool use_stdout = false;

    double temp_hz = 0.0;
    double pressure_hz = 0.0;
    double humidity_hz = 0.0;
    double position_hz = 0.0;
    double wind_hz = 0.0;

    GeneratorKind gen = GeneratorKind::RandomWalk;
    std::uint32_t seed = 1;
    double duration_sec = 0.0;
    double log_every_sec = 5.0;

    double bad_checksum_percent = 0.0;
    double truncated_percent = 0.0;
};
}
