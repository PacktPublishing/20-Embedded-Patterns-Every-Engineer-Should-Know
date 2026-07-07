// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cstdint>
#include <vector>

#include <sys/epoll.h>

namespace pbook
{

class IReadableSource;

class EpollReactor
{
public:
    explicit EpollReactor(int maxEvents = 16);
    ~EpollReactor();

    EpollReactor(const EpollReactor&) = delete;
    EpollReactor& operator=(const EpollReactor&) = delete;

    EpollReactor(EpollReactor&& other) noexcept;
    EpollReactor& operator=(EpollReactor&& other) noexcept;

    bool open() noexcept;
    bool is_open() const noexcept { return m_epollFd >= 0; }

    bool add(IReadableSource& source, std::uint32_t events = EPOLLIN) noexcept;
    bool remove(IReadableSource& source) noexcept;

    // Returns the number of events dispatched, zero on timeout, or -1 on error.
    // EINTR is treated as a harmless zero-dispatch wakeup.
    int poll_once(int timeoutMs) noexcept;

private:
    int m_epollFd = -1;
    std::vector<epoll_event> m_events;
};

} // namespace pbook
