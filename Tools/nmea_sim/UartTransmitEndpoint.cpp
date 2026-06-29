#include "UartTransmitEndpoint.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace nmea_sim
{
static speed_t to_speed(int baud) noexcept
{
    switch (baud)
    {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return 0;
    }
}

UartTransmitEndpoint::UartTransmitEndpoint(std::string device, int baud)
    : m_device(std::move(device)), m_baud(baud) {}

UartTransmitEndpoint::~UartTransmitEndpoint()
{
    if (m_fd >= 0) ::close(m_fd);
}

bool UartTransmitEndpoint::open() noexcept
{
    m_fd = ::open(m_device.c_str(), O_WRONLY | O_NOCTTY | O_CLOEXEC);
    if (m_fd < 0) return false;

    termios tio{};
    if (tcgetattr(m_fd, &tio) != 0) return false;
    const speed_t spd = to_speed(m_baud);
    if (spd == 0) return false;

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CRTSCTS;
    if (cfsetispeed(&tio, spd) != 0 || cfsetospeed(&tio, spd) != 0) return false;
    if (tcsetattr(m_fd, TCSANOW, &tio) != 0) return false;
    return true;
}

bool UartTransmitEndpoint::send(std::string_view bytes) noexcept
{
    if (m_fd < 0) return false;
    const char* p = bytes.data();
    std::size_t remaining = bytes.size();
    while (remaining > 0)
    {
        const auto n = ::write(m_fd, p, remaining);
        if (n <= 0) return false;
        p += static_cast<std::size_t>(n);
        remaining -= static_cast<std::size_t>(n);
    }
    return true;
}
}
