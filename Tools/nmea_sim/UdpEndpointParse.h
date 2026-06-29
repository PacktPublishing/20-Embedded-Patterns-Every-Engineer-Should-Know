#pragma once
#include <cstdlib>
#include <string>
#include "Options.h"

namespace nmea_sim
{
inline bool parse_udp_endpoint(const std::string& s, UdpEndpoint& ep) noexcept
{
    const auto pos = s.find(':');
    if (pos == std::string::npos) return false;
    const std::string host = s.substr(0, pos);
    const std::string portStr = s.substr(pos + 1);
    if (host.empty() || portStr.empty()) return false;

    char* end = nullptr;
    const long portLong = std::strtol(portStr.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || portLong <= 0 || portLong > 65535) return false;

    ep.host = host;
    ep.port = static_cast<std::uint16_t>(portLong);
    return true;
}
}
