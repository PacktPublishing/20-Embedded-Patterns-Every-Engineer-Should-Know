// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "heap_monitor/HeapMonitor.h"

#include <cstddef>
#include <cstdint>
#include <new>

namespace
{

bool test_scalar_allocation()
{
    const auto before = heap_monitor::statistics();
    auto* value = new std::uint64_t{42};
    const auto allocated = heap_monitor::statistics();

    if (allocated.allocation_count != before.allocation_count + 1 ||
        allocated.allocated_bytes !=
            before.allocated_bytes + sizeof(std::uint64_t) ||
        allocated.live_bytes != before.live_bytes + sizeof(std::uint64_t))
    {
        delete value;
        return false;
    }

    delete value;
    const auto after = heap_monitor::statistics();

    return after.deallocation_count == before.deallocation_count + 1 &&
           after.deallocated_bytes ==
               before.deallocated_bytes + sizeof(std::uint64_t) &&
           after.live_bytes == before.live_bytes;
}

bool test_array_allocation()
{
    constexpr std::size_t element_count = 13;
    constexpr std::size_t requested_bytes =
        element_count * sizeof(std::uint32_t);

    const auto before = heap_monitor::statistics();
    auto* values = new std::uint32_t[element_count];
    const auto allocated = heap_monitor::statistics();
    delete[] values;
    const auto after = heap_monitor::statistics();

    return allocated.allocation_count == before.allocation_count + 1 &&
           allocated.allocated_bytes ==
               before.allocated_bytes + requested_bytes &&
           after.deallocation_count == before.deallocation_count + 1 &&
           after.live_bytes == before.live_bytes;
}

struct alignas(64) AlignedValue
{
    std::byte bytes[64];
};

bool test_aligned_allocation()
{
    const auto before = heap_monitor::statistics();
    auto* value = new AlignedValue{};
    const auto allocated = heap_monitor::statistics();

    const auto address = reinterpret_cast<std::uintptr_t>(value);
    const bool aligned = address % alignof(AlignedValue) == 0;

    delete value;
    const auto after = heap_monitor::statistics();

    return aligned &&
           allocated.allocation_count == before.allocation_count + 1 &&
           allocated.allocated_bytes ==
               before.allocated_bytes + sizeof(AlignedValue) &&
           after.live_bytes == before.live_bytes;
}

bool test_nothrow_allocation()
{
    const auto before = heap_monitor::statistics();
    auto* value = new (std::nothrow) std::uint32_t{7};
    if (value == nullptr)
    {
        return false;
    }

    const auto allocated = heap_monitor::statistics();
    delete value;

    return allocated.allocation_count == before.allocation_count + 1;
}

bool test_steady_state_tracking()
{
    const auto before = heap_monitor::statistics();
    heap_monitor::begin_steady_state();

    auto* values = new std::uint16_t[8];
    const auto allocated = heap_monitor::statistics();
    delete[] values;

    return heap_monitor::is_steady_state() &&
           allocated.steady_state_allocation_count ==
               before.steady_state_allocation_count + 1 &&
           allocated.steady_state_allocated_bytes ==
               before.steady_state_allocated_bytes +
                   (8 * sizeof(std::uint16_t));
}

} // namespace

int main()
{
    if (!test_scalar_allocation())
    {
        return 1;
    }

    if (!test_array_allocation())
    {
        return 2;
    }

    if (!test_aligned_allocation())
    {
        return 3;
    }

    if (!test_nothrow_allocation())
    {
        return 4;
    }

    if (!test_steady_state_tracking())
    {
        return 5;
    }

    return 0;
}
