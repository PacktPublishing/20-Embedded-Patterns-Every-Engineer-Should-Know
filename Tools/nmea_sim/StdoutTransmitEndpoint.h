#pragma once
#include "ITransmitEndpoint.h"

namespace nmea_sim
{
class StdoutTransmitEndpoint final : public ITransmitEndpoint
{
public:
    bool send(std::string_view bytes) noexcept override;
};
}
