// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cstdint>

namespace pbook
{

struct PortStats
{
    std::uint64_t bytes_received = 0;
    std::uint64_t frames_received = 0;
    std::uint64_t frames_dropped = 0;
    std::uint64_t read_errors = 0;
    std::uint64_t overflow_errors = 0;
};

} // namespace pbook
