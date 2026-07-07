// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "UdpDatagramReceiver.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace pbook
{

namespace
{

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

UdpDatagramReceiver::UdpDatagramReceiver(std::uint16_t port,
                                         Handler handler,
                                         void* userData,
                                         std::size_t maxDatagramBytes)
    : m_requestedPort(port)
    , m_handler(handler)
    , m_userData(userData)
    , m_buffer(maxDatagramBytes == 0 ? 1 : maxDatagramBytes)
{
}

UdpDatagramReceiver::~UdpDatagramReceiver()
{
    close_fd(m_fd);
}

bool UdpDatagramReceiver::open() noexcept
{
    if (m_fd >= 0)
    {
        return true;
    }

    m_fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (m_fd < 0)
    {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_requestedPort);

    if (::bind(m_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        close_fd(m_fd);
        return false;
    }

    sockaddr_in actual{};
    socklen_t actualLen = sizeof(actual);
    if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&actual), &actualLen) == 0)
    {
        m_localPort = ntohs(actual.sin_port);
    }

    if (!set_nonblocking(m_fd))
    {
        close_fd(m_fd);
        return false;
    }

    return true;
}

void UdpDatagramReceiver::on_readable() noexcept
{
    if (m_fd < 0)
    {
        return;
    }

    for (;;)
    {
        sockaddr_in src{};
        socklen_t srcLen = sizeof(src);

        const ssize_t n = ::recvfrom(
            m_fd,
            m_buffer.data(),
            m_buffer.size(),
            0,
            reinterpret_cast<sockaddr*>(&src),
            &srcLen);

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
        ++m_stats.frames_received;

        if (m_handler != nullptr)
        {
            m_handler(ImmutableByteView{m_buffer.data(), count}, src, m_userData);
        }
    }
}

} // namespace pbook
