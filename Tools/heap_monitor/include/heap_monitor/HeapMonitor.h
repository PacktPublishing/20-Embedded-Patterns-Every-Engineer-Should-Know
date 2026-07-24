// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cstddef>
#include <cstdint>

namespace heap_monitor
{

struct Statistics
{
    std::uint64_t allocation_count;
    std::uint64_t deallocation_count;
    std::uint64_t allocated_bytes;
    std::uint64_t deallocated_bytes;
    std::uint64_t live_bytes;
    std::uint64_t peak_live_bytes;
    std::uint64_t steady_state_allocation_count;
    std::uint64_t steady_state_allocated_bytes;
};

// Return a consistent-enough snapshot of the independent atomic counters.
// Another thread may allocate while the snapshot is being collected.
[[nodiscard]] Statistics statistics() noexcept;

// Mark the end of initialization. Subsequent allocations are counted as
// steady-state allocations.
void begin_steady_state() noexcept;

[[nodiscard]] bool is_steady_state() noexcept;

} // namespace heap_monitor
