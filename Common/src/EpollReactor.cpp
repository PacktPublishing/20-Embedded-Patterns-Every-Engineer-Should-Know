// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "EpollReactor.h"
#include "IReadableSource.h"

#include <cerrno>
#include <unistd.h>
#include <utility>

namespace pbook
{

EpollReactor::EpollReactor(int maxEvents)
    : m_events(static_cast<std::size_t>(maxEvents > 0 ? maxEvents : 1))
{
}

EpollReactor::~EpollReactor()
{
    if (m_epollFd >= 0)
    {
        ::close(m_epollFd);
        m_epollFd = -1;
    }
}

EpollReactor::EpollReactor(EpollReactor&& other) noexcept
    : m_epollFd(other.m_epollFd)
    , m_events(std::move(other.m_events))
{
    other.m_epollFd = -1;
}

EpollReactor& EpollReactor::operator=(EpollReactor&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (m_epollFd >= 0)
    {
        ::close(m_epollFd);
    }

    m_epollFd = other.m_epollFd;
    m_events = std::move(other.m_events);
    other.m_epollFd = -1;
    return *this;
}

bool EpollReactor::open() noexcept
{
    if (m_epollFd >= 0)
    {
        return true;
    }

    m_epollFd = ::epoll_create1(EPOLL_CLOEXEC);
    return m_epollFd >= 0;
}

bool EpollReactor::add(IReadableSource& source, std::uint32_t events) noexcept
{
    if (m_epollFd < 0)
    {
        return false;
    }

    const int sourceFd = source.fd();
    if (sourceFd < 0)
    {
        return false;
    }

    epoll_event ev{};
    ev.events = events;
    ev.data.ptr = &source;

    return ::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, sourceFd, &ev) == 0;
}

bool EpollReactor::remove(IReadableSource& source) noexcept
{
    if (m_epollFd < 0)
    {
        return false;
    }

    const int sourceFd = source.fd();
    if (sourceFd < 0)
    {
        return false;
    }

    return ::epoll_ctl(m_epollFd, EPOLL_CTL_DEL, sourceFd, nullptr) == 0;
}

int EpollReactor::poll_once(int timeoutMs) noexcept
{
    if (m_epollFd < 0 || m_events.empty())
    {
        return -1;
    }

    const int n = ::epoll_wait(
        m_epollFd,
        m_events.data(),
        static_cast<int>(m_events.size()),
        timeoutMs);

    if (n < 0)
    {
        if (errno == EINTR)
        {
            return 0;
        }
        return -1;
    }

    for (int i = 0; i < n; ++i)
    {
        auto* source = static_cast<IReadableSource*>(
            m_events[static_cast<std::size_t>(i)].data.ptr);

        if (source != nullptr)
        {
            source->on_readable();
        }
    }

    return n;
}

} // namespace pbook
