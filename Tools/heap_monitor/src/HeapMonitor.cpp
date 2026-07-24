// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "heap_monitor/HeapMonitor.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace
{

struct AllocationHeader
{
    void* allocation;
    std::size_t requested_bytes;
};

std::atomic<std::uint64_t> allocation_count{0};
std::atomic<std::uint64_t> deallocation_count{0};
std::atomic<std::uint64_t> allocated_bytes{0};
std::atomic<std::uint64_t> deallocated_bytes{0};
std::atomic<std::uint64_t> live_bytes{0};
std::atomic<std::uint64_t> peak_live_bytes{0};
std::atomic<std::uint64_t> steady_state_allocation_count{0};
std::atomic<std::uint64_t> steady_state_allocated_bytes{0};
std::atomic<bool> steady_state{false};

void update_peak(std::uint64_t candidate) noexcept
{
    auto peak = peak_live_bytes.load(std::memory_order_relaxed);

    while (candidate > peak &&
           !peak_live_bytes.compare_exchange_weak(
               peak,
               candidate,
               std::memory_order_relaxed,
               std::memory_order_relaxed))
    {
    }
}

void record_allocation(std::size_t bytes) noexcept
{
    const auto byte_count = static_cast<std::uint64_t>(bytes);

    allocation_count.fetch_add(1, std::memory_order_relaxed);
    allocated_bytes.fetch_add(byte_count, std::memory_order_relaxed);

    const auto current =
        live_bytes.fetch_add(byte_count, std::memory_order_relaxed) +
        byte_count;
    update_peak(current);

    if (steady_state.load(std::memory_order_relaxed))
    {
        steady_state_allocation_count.fetch_add(
            1,
            std::memory_order_relaxed);
        steady_state_allocated_bytes.fetch_add(
            byte_count,
            std::memory_order_relaxed);
    }
}

void record_deallocation(std::size_t bytes) noexcept
{
    const auto byte_count = static_cast<std::uint64_t>(bytes);

    deallocation_count.fetch_add(1, std::memory_order_relaxed);
    deallocated_bytes.fetch_add(byte_count, std::memory_order_relaxed);
    live_bytes.fetch_sub(byte_count, std::memory_order_relaxed);
}

void* allocate(std::size_t requested_bytes, std::size_t alignment)
{
    const auto payload_bytes = requested_bytes == 0 ? 1 : requested_bytes;
    const auto overhead = sizeof(AllocationHeader) + alignment - 1;

    if (payload_bytes > std::numeric_limits<std::size_t>::max() - overhead)
    {
        throw std::bad_alloc{};
    }

    void* allocation = std::malloc(payload_bytes + overhead);
    if (allocation == nullptr)
    {
        throw std::bad_alloc{};
    }

    const auto start =
        reinterpret_cast<std::uintptr_t>(allocation) +
        sizeof(AllocationHeader);
    const auto aligned_address =
        (start + alignment - 1) & ~(alignment - 1);
    auto* header = reinterpret_cast<AllocationHeader*>(aligned_address) - 1;

    header->allocation = allocation;
    header->requested_bytes = requested_bytes;
    record_allocation(requested_bytes);

    return reinterpret_cast<void*>(aligned_address);
}

void deallocate(void* pointer) noexcept
{
    if (pointer == nullptr)
    {
        return;
    }

    const auto* header =
        reinterpret_cast<const AllocationHeader*>(pointer) - 1;

    record_deallocation(header->requested_bytes);
    std::free(header->allocation);
}

} // namespace

namespace heap_monitor
{

Statistics statistics() noexcept
{
    return Statistics{
        allocation_count.load(std::memory_order_relaxed),
        deallocation_count.load(std::memory_order_relaxed),
        allocated_bytes.load(std::memory_order_relaxed),
        deallocated_bytes.load(std::memory_order_relaxed),
        live_bytes.load(std::memory_order_relaxed),
        peak_live_bytes.load(std::memory_order_relaxed),
        steady_state_allocation_count.load(std::memory_order_relaxed),
        steady_state_allocated_bytes.load(std::memory_order_relaxed)};
}

void begin_steady_state() noexcept
{
    steady_state.store(true, std::memory_order_relaxed);
}

bool is_steady_state() noexcept
{
    return steady_state.load(std::memory_order_relaxed);
}

} // namespace heap_monitor

void* operator new(std::size_t bytes)
{
    return allocate(bytes, alignof(std::max_align_t));
}

void* operator new[](std::size_t bytes)
{
    return allocate(bytes, alignof(std::max_align_t));
}

void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept
{
    try
    {
        return allocate(bytes, alignof(std::max_align_t));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept
{
    try
    {
        return allocate(bytes, alignof(std::max_align_t));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new(std::size_t bytes, std::align_val_t alignment)
{
    return allocate(bytes, static_cast<std::size_t>(alignment));
}

void* operator new[](
    std::size_t bytes,
    std::align_val_t alignment)
{
    return allocate(bytes, static_cast<std::size_t>(alignment));
}

void* operator new(
    std::size_t bytes,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept
{
    try
    {
        return allocate(bytes, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](
    std::size_t bytes,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept
{
    try
    {
        return allocate(bytes, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* pointer) noexcept
{
    deallocate(pointer);
}

void operator delete[](void* pointer) noexcept
{
    deallocate(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    deallocate(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept
{
    deallocate(pointer);
}

void operator delete(
    void* pointer,
    const std::nothrow_t&) noexcept
{
    deallocate(pointer);
}

void operator delete[](
    void* pointer,
    const std::nothrow_t&) noexcept
{
    deallocate(pointer);
}

void operator delete(void* pointer, std::align_val_t) noexcept
{
    deallocate(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept
{
    deallocate(pointer);
}

void operator delete(
    void* pointer,
    std::size_t,
    std::align_val_t) noexcept
{
    deallocate(pointer);
}

void operator delete[](
    void* pointer,
    std::size_t,
    std::align_val_t) noexcept
{
    deallocate(pointer);
}

void operator delete(
    void* pointer,
    std::align_val_t,
    const std::nothrow_t&) noexcept
{
    deallocate(pointer);
}

void operator delete[](
    void* pointer,
    std::align_val_t,
    const std::nothrow_t&) noexcept
{
    deallocate(pointer);
}
