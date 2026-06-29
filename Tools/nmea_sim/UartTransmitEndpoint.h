#pragma once
#include "ITransmitEndpoint.h"
#include <string>

namespace nmea_sim
{
class UartTransmitEndpoint final : public ITransmitEndpoint
{
public:
    UartTransmitEndpoint(std::string device, int baud);
    ~UartTransmitEndpoint() override;
    UartTransmitEndpoint(const UartTransmitEndpoint&) = delete;
    UartTransmitEndpoint& operator=(const UartTransmitEndpoint&) = delete;
    bool open() noexcept;
    bool send(std::string_view bytes) noexcept override;
private:
    int m_fd = -1;
    std::string m_device;
    int m_baud = 0;
};
}
