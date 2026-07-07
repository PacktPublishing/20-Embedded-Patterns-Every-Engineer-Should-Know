// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "TimerFdSource.h"

#include <cerrno>
#include <cstring>
#include <sys/timerfd.h>
#include <unistd.h>

namespace pbook
{

namespace
{

void close_fd(int& fd) noexcept
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

timespec to_timespec(std::chrono::milliseconds duration) noexcept
{
    if (duration.count() < 0)
    {
        duration = std::chrono::milliseconds{0};
    }

    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    const auto remainder = duration - seconds;

    timespec ts{};
    ts.tv_sec = static_cast<time_t>(seconds.count());
    ts.tv_nsec = static_cast<long>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(remainder).count());
    return ts;
}

} // namespace

TimerFdSource::TimerFdSource(Handler handler, void* userData)
    : m_handler(handler)
    , m_userData(userData)
{
}

TimerFdSource::~TimerFdSource()
{
    close_fd(m_fd);
}

bool TimerFdSource::open() noexcept
{
    if (m_fd >= 0)
    {
        return true;
    }

    m_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    return m_fd >= 0;
}

bool TimerFdSource::arm_periodic(std::chrono::milliseconds interval) noexcept
{
    if (interval.count() <= 0)
    {
        return false;
    }

    return set_time(interval, interval);
}

bool TimerFdSource::arm_oneshot(std::chrono::milliseconds delay) noexcept
{
    if (delay.count() <= 0)
    {
        return false;
    }

    return set_time(delay, std::chrono::milliseconds{0});
}

bool TimerFdSource::disarm() noexcept
{
    return set_time(std::chrono::milliseconds{0}, std::chrono::milliseconds{0});
}

bool TimerFdSource::set_time(std::chrono::milliseconds first,
                             std::chrono::milliseconds interval) noexcept
{
    if (!open())
    {
        return false;
    }

    itimerspec spec{};
    spec.it_value = to_timespec(first);
    spec.it_interval = to_timespec(interval);

    return ::timerfd_settime(m_fd, 0, &spec, nullptr) == 0;
}

void TimerFdSource::on_readable() noexcept
{
    if (m_fd < 0)
    {
        return;
    }

    for (;;)
    {
        std::uint64_t expirations = 0;
        const ssize_t n = ::read(m_fd, &expirations, sizeof(expirations));

        if (n == static_cast<ssize_t>(sizeof(expirations)))
        {
            m_totalExpirations += expirations;

            if (m_handler != nullptr)
            {
                m_handler(expirations, m_userData);
            }
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        {
            return;
        }

        ++m_readErrors;
        return;
    }
}

} // namespace pbook
