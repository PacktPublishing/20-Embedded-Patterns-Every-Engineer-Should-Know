#include "StdoutTransmitEndpoint.h"
#include <iostream>

namespace nmea_sim
{
bool StdoutTransmitEndpoint::send(std::string_view bytes) noexcept
{
    std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return std::cout.good();
}
}
