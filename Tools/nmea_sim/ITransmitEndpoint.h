#pragma once
#include <string_view>

namespace nmea_sim
{
class ITransmitEndpoint
{
public:
    virtual ~ITransmitEndpoint() = default;
    virtual bool send(std::string_view bytes) noexcept = 0;
};
}
