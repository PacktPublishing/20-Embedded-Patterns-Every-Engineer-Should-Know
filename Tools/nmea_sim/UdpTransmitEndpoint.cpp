#include "UdpTransmitEndpoint.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

namespace nmea_sim
{
UdpTransmitEndpoint::UdpTransmitEndpoint(std::string host, int port)
    : m_host(std::move(host)), m_port(port)
{
    std::memset(&m_addr, 0, sizeof(m_addr));
}

UdpTransmitEndpoint::~UdpTransmitEndpoint()
{
    if (m_fd >= 0) ::close(m_fd);
}

bool UdpTransmitEndpoint::open() noexcept
{
    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_fd < 0) return false;
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(static_cast<std::uint16_t>(m_port));
    if (::inet_pton(AF_INET, m_host.c_str(), &m_addr.sin_addr) != 1)
    {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    return true;
}

bool UdpTransmitEndpoint::send(std::string_view bytes) noexcept
{
    if (m_fd < 0) return false;
    const auto n = ::sendto(m_fd, bytes.data(), bytes.size(), 0,
        reinterpret_cast<const sockaddr*>(&m_addr), sizeof(m_addr));
    return n == static_cast<ssize_t>(bytes.size());
}
}
