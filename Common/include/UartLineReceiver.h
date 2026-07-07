// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "IReadableSource.h"
#include "PortStats.h"

namespace pbook
{

class UartLineReceiver final : public IReadableSource
{
public:
    // The delivered line does not include CR or LF terminators.
    using Handler = void (*)(std::string_view line, void* userData) noexcept;

    explicit UartLineReceiver(std::string device,
                              int baud,
                              Handler handler,
                              void* userData = nullptr,
                              std::size_t maxLineBytes = 128);
    ~UartLineReceiver() override;

    UartLineReceiver(const UartLineReceiver&) = delete;
    UartLineReceiver& operator=(const UartLineReceiver&) = delete;

    bool open() noexcept;

    int fd() const noexcept override { return m_fd; }
    void on_readable() noexcept override;

    const std::string& device() const noexcept { return m_device; }
    int baud() const noexcept { return m_baud; }
    const PortStats& stats() const noexcept { return m_stats; }

private:
    bool accept_byte(char c) noexcept;
    void reset_line() noexcept;

    int m_fd = -1;
    std::string m_device;
    int m_baud = 0;
    Handler m_handler = nullptr;
    void* m_userData = nullptr;
    std::size_t m_maxLineBytes = 128;
    std::vector<char> m_line;
    PortStats m_stats{};
};

} // namespace pbook
