// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "SignalFdSource.h"

#include <cerrno>
#include <sys/signalfd.h>
#include <unistd.h>
#include <utility>

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

} // namespace

SignalFdSource::SignalFdSource(std::initializer_list<int> signals,
                               Handler handler,
                               void* userData)
    : m_signals(signals)
    , m_handler(handler)
    , m_userData(userData)
{
}

SignalFdSource::SignalFdSource(std::vector<int> signals,
                               Handler handler,
                               void* userData)
    : m_signals(std::move(signals))
    , m_handler(handler)
    , m_userData(userData)
{
}

SignalFdSource::~SignalFdSource()
{
    close_fd(m_fd);
}

bool SignalFdSource::build_signal_set(sigset_t& set) const noexcept
{
    if (::sigemptyset(&set) != 0)
    {
        return false;
    }

    for (int signo : m_signals)
    {
        if (::sigaddset(&set, signo) != 0)
        {
            return false;
        }
    }

    return true;
}

bool SignalFdSource::open() noexcept
{
    if (m_fd >= 0)
    {
        return true;
    }

    sigset_t set{};
    if (!build_signal_set(set))
    {
        return false;
    }

    // signalfd only receives signals that are blocked from normal delivery.
    // This process-level mask change is intentional for small event-loop programs.
    if (::sigprocmask(SIG_BLOCK, &set, nullptr) != 0)
    {
        return false;
    }

    m_fd = ::signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);
    return m_fd >= 0;
}

void SignalFdSource::on_readable() noexcept
{
    if (m_fd < 0)
    {
        return;
    }

    for (;;)
    {
        signalfd_siginfo info{};
        const ssize_t n = ::read(m_fd, &info, sizeof(info));

        if (n == static_cast<ssize_t>(sizeof(info)))
        {
            ++m_signalsReceived;

            if (m_handler != nullptr)
            {
                m_handler(static_cast<int>(info.ssi_signo), m_userData);
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
