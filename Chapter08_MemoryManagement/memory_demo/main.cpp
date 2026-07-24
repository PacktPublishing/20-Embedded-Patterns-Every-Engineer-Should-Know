// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "heap_monitor/HeapMonitor.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory_resource>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace
{

constexpr std::size_t work_unit_count = 10'000;
constexpr std::size_t resource_bytes = 16 * 1024;

struct Message
{
    std::array<std::byte, 64> payload{};
    std::size_t sequence{};
};

struct ScenarioResult
{
    const char* name;
    std::size_t work_units;
    bool expected_behavior;
    const char* expectation;
};

void print_report(const ScenarioResult& scenario)
{
    const auto stats = heap_monitor::statistics();
    const auto initialization_allocations =
        stats.allocation_count - stats.steady_state_allocation_count;
    const auto initialization_bytes =
        stats.allocated_bytes - stats.steady_state_allocated_bytes;

    std::printf(
        "Scenario: %s\n"
        "Work units processed:              %zu\n\n"
        "Initialization\n"
        "  Allocations:                     %llu\n"
        "  Requested bytes:                 %llu\n\n"
        "Steady state\n"
        "  Allocations:                     %llu\n"
        "  Requested bytes:                 %llu\n\n"
        "Overall\n"
        "  Deallocations:                   %llu\n"
        "  Live requested bytes:            %llu\n"
        "  Peak live requested bytes:       %llu\n\n"
        "Result: %s - %s\n",
        scenario.name,
        scenario.work_units,
        static_cast<unsigned long long>(initialization_allocations),
        static_cast<unsigned long long>(initialization_bytes),
        static_cast<unsigned long long>(
            stats.steady_state_allocation_count),
        static_cast<unsigned long long>(stats.steady_state_allocated_bytes),
        static_cast<unsigned long long>(stats.deallocation_count),
        static_cast<unsigned long long>(stats.live_bytes),
        static_cast<unsigned long long>(stats.peak_live_bytes),
        scenario.expected_behavior ? "PASS" : "FAIL",
        scenario.expectation);
}

ScenarioResult run_reserved()
{
    std::vector<Message> messages;
    messages.reserve(128);

    heap_monitor::begin_steady_state();

    std::size_t completed = 0;
    for (std::size_t index = 0; index < work_unit_count; ++index)
    {
        messages.push_back(Message{{}, index});
        if (messages.size() == messages.capacity())
        {
            messages.clear();
        }
        ++completed;
    }

    const auto stats = heap_monitor::statistics();
    return {
        "reserved vector",
        completed,
        stats.steady_state_allocation_count == 0,
        "no steady-state heap allocations"};
}

ScenarioResult run_growing()
{
    std::vector<Message> messages;

    heap_monitor::begin_steady_state();

    constexpr std::size_t message_count = 1'000;
    for (std::size_t index = 0; index < message_count; ++index)
    {
        messages.push_back(Message{{}, index});
    }

    const auto stats = heap_monitor::statistics();
    return {
        "growing vector",
        message_count,
        stats.steady_state_allocation_count > 0,
        "intentional steady-state allocations detected"};
}

ScenarioResult run_pool()
{
    std::array<std::byte, resource_bytes> storage{};
    std::pmr::monotonic_buffer_resource backing_resource{
        storage.data(),
        storage.size(),
        std::pmr::null_memory_resource()};
    std::pmr::unsynchronized_pool_resource message_pool{&backing_resource};
    std::pmr::vector<Message> messages{&message_pool};
    messages.reserve(128);

    heap_monitor::begin_steady_state();

    std::size_t completed = 0;
    for (std::size_t index = 0; index < work_unit_count; ++index)
    {
        messages.push_back(Message{{}, index});
        if (messages.size() == messages.capacity())
        {
            messages.clear();
        }
        ++completed;
    }

    const auto stats = heap_monitor::statistics();
    return {
        "bounded PMR pool",
        completed,
        stats.steady_state_allocation_count == 0,
        "no steady-state global heap allocations"};
}

ScenarioResult run_arena()
{
    std::array<std::byte, resource_bytes> storage{};
    std::pmr::monotonic_buffer_resource arena{
        storage.data(),
        storage.size(),
        std::pmr::null_memory_resource()};

    heap_monitor::begin_steady_state();

    constexpr std::size_t cycles = 100;
    std::size_t completed = 0;
    for (std::size_t cycle = 0; cycle < cycles; ++cycle)
    {
        {
            std::pmr::vector<int> values{&arena};
            std::pmr::string text{&arena};
            values.reserve(128);
            text.reserve(128);

            for (int value = 0; value < 128; ++value)
            {
                values.push_back(value);
                text.push_back(static_cast<char>('A' + (value % 26)));
                ++completed;
            }
        }
        arena.release();
    }

    const auto stats = heap_monitor::statistics();
    return {
        "bounded PMR arena",
        completed,
        stats.steady_state_allocation_count == 0,
        "arena reused without global heap allocations"};
}

ScenarioResult run_exhaustion()
{
    std::array<std::byte, 512> storage{};
    std::pmr::monotonic_buffer_resource arena{
        storage.data(),
        storage.size(),
        std::pmr::null_memory_resource()};

    heap_monitor::begin_steady_state();

    bool exhausted = false;
    std::size_t completed = 0;
    try
    {
        std::pmr::vector<std::byte> bytes{&arena};
        for (;;)
        {
            bytes.push_back(std::byte{0x5a});
            ++completed;
        }
    }
    catch (const std::bad_alloc&)
    {
        exhausted = true;
    }

    const auto stats = heap_monitor::statistics();
    return {
        "bounded arena exhaustion",
        completed,
        exhausted && stats.steady_state_allocation_count == 0,
        "capacity limit enforced without heap fallback"};
}

void print_usage(const char* executable)
{
    std::fprintf(
        stderr,
        "Usage: %s reserved|growing|pool|arena|exhaustion\n",
        executable);
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        print_usage(argv[0]);
        return 2;
    }

    const std::string_view scenario{argv[1]};
    ScenarioResult result{};

    if (scenario == "reserved")
    {
        result = run_reserved();
    }
    else if (scenario == "growing")
    {
        result = run_growing();
    }
    else if (scenario == "pool")
    {
        result = run_pool();
    }
    else if (scenario == "arena")
    {
        result = run_arena();
    }
    else if (scenario == "exhaustion")
    {
        result = run_exhaustion();
    }
    else
    {
        print_usage(argv[0]);
        return 2;
    }

    print_report(result);
    return result.expected_behavior ? 0 : 1;
}
