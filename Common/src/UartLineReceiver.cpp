// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "UartLineReceiver.h"

#include <array>
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <utility>

namespace pbook
{

namespace
{

speed_t baud_to_speed(int baud) noexcept
{
    switch (baud)
    {
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    default: return 0;
    }
}

bool set_nonblocking(int fd) noexcept
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return false;
    }

    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void close_fd(int& fd) noexcept
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

} // namespace

UartLineReceiver::UartLineReceiver(std::string device,
                                   int baud,
                                   Handler handler,
                                   void* userData,
                                   std::size_t maxLineBytes)
    : m_device(std::move(device))
    , m_baud(baud)
    , m_handler(handler)
    , m_userData(userData)
    , m_maxLineBytes(maxLineBytes == 0 ? 1 : maxLineBytes)
{
    m_line.reserve(m_maxLineBytes);
}

UartLineReceiver::~UartLineReceiver()
{
    close_fd(m_fd);
}

bool UartLineReceiver::open() noexcept
{
    if (m_fd >= 0)
    {
        return true;
    }

    const speed_t speed = baud_to_speed(m_baud);
    if (speed == 0)
    {
        return false;
    }

    m_fd = ::open(m_device.c_str(), O_RDONLY | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
    if (m_fd < 0)
    {
        return false;
    }

    termios tio{};
    if (::tcgetattr(m_fd, &tio) != 0)
    {
        close_fd(m_fd);
        return false;
    }

    ::cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (::cfsetispeed(&tio, speed) != 0 || ::cfsetospeed(&tio, speed) != 0)
    {
        close_fd(m_fd);
        return false;
    }

    if (::tcsetattr(m_fd, TCSANOW, &tio) != 0)
    {
        close_fd(m_fd);
        return false;
    }

    if (!set_nonblocking(m_fd))
    {
        close_fd(m_fd);
        return false;
    }

    return true;
}

void UartLineReceiver::reset_line() noexcept
{
    m_line.clear();
}

bool UartLineReceiver::accept_byte(char c) noexcept
{
    if (c == '\r')
    {
        return false;
    }

    if (c == '\n')
    {
        return !m_line.empty();
    }

    if (m_line.size() >= m_maxLineBytes)
    {
        ++m_stats.frames_dropped;
        ++m_stats.overflow_errors;
        reset_line();
        return false;
    }

    m_line.push_back(c);
    return false;
}

void UartLineReceiver::on_readable() noexcept
{
    if (m_fd < 0)
    {
        return;
    }

    std::array<char, 256> rx{};

    for (;;)
    {
        const ssize_t n = ::read(m_fd, rx.data(), rx.size());
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }

            ++m_stats.read_errors;
            return;
        }

        if (n == 0)
        {
            return;
        }

        const auto count = static_cast<std::size_t>(n);
        m_stats.bytes_received += count;

        for (std::size_t i = 0; i < count; ++i)
        {
            if (!accept_byte(rx[i]))
            {
                continue;
            }

            ++m_stats.frames_received;

            if (m_handler != nullptr)
            {
                m_handler(std::string_view{m_line.data(), m_line.size()}, m_userData);
            }

            reset_line();
        }
    }
}

} // namespace pbook
