// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include "IReadableSource.h"

#include <chrono>
#include <cstdint>

namespace pbook
{

class TimerFdSource final : public IReadableSource
{
public:
    using Handler = void (*)(std::uint64_t expirations, void* userData) noexcept;

    explicit TimerFdSource(Handler handler, void* userData = nullptr);
    ~TimerFdSource() override;

    TimerFdSource(const TimerFdSource&) = delete;
    TimerFdSource& operator=(const TimerFdSource&) = delete;

    bool open() noexcept;
    bool is_open() const noexcept { return m_fd >= 0; }

    bool arm_periodic(std::chrono::milliseconds interval) noexcept;
    bool arm_oneshot(std::chrono::milliseconds delay) noexcept;
    bool disarm() noexcept;

    int fd() const noexcept override { return m_fd; }
    void on_readable() noexcept override;

    std::uint64_t total_expirations() const noexcept { return m_totalExpirations; }
    std::uint64_t read_errors() const noexcept { return m_readErrors; }

private:
    bool set_time(std::chrono::milliseconds first,
                  std::chrono::milliseconds interval) noexcept;

    int m_fd = -1;
    Handler m_handler = nullptr;
    void* m_userData = nullptr;
    std::uint64_t m_totalExpirations = 0;
    std::uint64_t m_readErrors = 0;
};

} // namespace pbook
