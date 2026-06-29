#pragma once
#include "ITransmitEndpoint.h"
#include <netinet/in.h>
#include <string>

namespace nmea_sim
{
class UdpTransmitEndpoint final : public ITransmitEndpoint
{
public:
    UdpTransmitEndpoint(std::string host, int port);
    ~UdpTransmitEndpoint() override;
    UdpTransmitEndpoint(const UdpTransmitEndpoint&) = delete;
    UdpTransmitEndpoint& operator=(const UdpTransmitEndpoint&) = delete;
    bool open() noexcept;
    bool send(std::string_view bytes) noexcept override;
private:
    int m_fd = -1;
    sockaddr_in m_addr{};
    std::string m_host;
    int m_port = 0;
};
}
