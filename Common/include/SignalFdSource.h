// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include "IReadableSource.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

#include <signal.h>

namespace pbook
{

class SignalFdSource final : public IReadableSource
{
public:
    using Handler = void (*)(int signo, void* userData) noexcept;

    explicit SignalFdSource(std::initializer_list<int> signals,
                            Handler handler,
                            void* userData = nullptr);
    explicit SignalFdSource(std::vector<int> signals,
                            Handler handler,
                            void* userData = nullptr);
    ~SignalFdSource() override;

    SignalFdSource(const SignalFdSource&) = delete;
    SignalFdSource& operator=(const SignalFdSource&) = delete;

    bool open() noexcept;
    bool is_open() const noexcept { return m_fd >= 0; }

    int fd() const noexcept override { return m_fd; }
    void on_readable() noexcept override;

    std::uint64_t signals_received() const noexcept { return m_signalsReceived; }
    std::uint64_t read_errors() const noexcept { return m_readErrors; }

private:
    bool build_signal_set(sigset_t& set) const noexcept;

    int m_fd = -1;
    std::vector<int> m_signals;
    Handler m_handler = nullptr;
    void* m_userData = nullptr;
    std::uint64_t m_signalsReceived = 0;
    std::uint64_t m_readErrors = 0;
};

} // namespace pbook
