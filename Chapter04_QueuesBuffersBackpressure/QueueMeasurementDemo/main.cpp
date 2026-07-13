// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "BoundedQueue.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

struct WorkItem
{
    std::uint64_t sequence;
    Clock::time_point producedAt;
};

struct Statistics
{
    std::atomic<std::uint64_t> produced = 0;
    std::atomic<std::uint64_t> consumed = 0;
    std::atomic<std::size_t> highWaterMark = 0;
    std::atomic<std::int64_t> maximumLatencyUs = 0;
};

void updateHighWaterMark(
    std::atomic<std::size_t>& highWaterMark,
    std::size_t currentDepth)
{
    auto previous = highWaterMark.load();

    while (currentDepth > previous &&
           !highWaterMark.compare_exchange_weak(previous, currentDepth))
    {
    }
}

void updateMaximumLatency(
    std::atomic<std::int64_t>& maximumLatencyUs,
    std::int64_t currentLatencyUs)
{
    auto previous = maximumLatencyUs.load();

    while (currentLatencyUs > previous &&
           !maximumLatencyUs.compare_exchange_weak(
               previous,
               currentLatencyUs))
    {
    }
}

int main()
{
    constexpr std::size_t QueueCapacity = 1000;
    constexpr auto ProducerPeriod = 1ms;
    constexpr auto ConsumerPeriod = 1250us;
    constexpr auto TestDuration = 6s;

    BoundedQueue<WorkItem, QueueCapacity> queue;
    Statistics statistics;

    const auto testStart = Clock::now();
    const auto testEnd = testStart + TestDuration;

    std::thread producer(
        [&]()
        {
            std::uint64_t sequence = 0;
            auto nextRelease = Clock::now();

            while (Clock::now() < testEnd)
            {
                WorkItem item{
                    .sequence = sequence++,
                    .producedAt = Clock::now()
                };

                if (!queue.push(std::move(item)))
                {
                    break;
                }

                ++statistics.produced;

                updateHighWaterMark(
                    statistics.highWaterMark,
                    queue.size());

                nextRelease += ProducerPeriod;
                std::this_thread::sleep_until(nextRelease);
            }

            queue.stop();
        });

    std::thread consumer(
        [&]()
        {
            while (const auto item = queue.pop())
            {
                const auto latency =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        Clock::now() - item->producedAt);

                updateMaximumLatency(
                    statistics.maximumLatencyUs,
                    latency.count());

                ++statistics.consumed;
                std::this_thread::sleep_for(ConsumerPeriod);
            }
        });

    producer.join();
    consumer.join();

    const auto elapsed =
        std::chrono::duration<double>(Clock::now() - testStart).count();

    std::cout
        << "Produced:          " << statistics.produced.load() << '\n'
        << "Consumed:          " << statistics.consumed.load() << '\n'
        << "High-water mark:   " << statistics.highWaterMark.load() << '\n'
        << "Maximum latency:   " << statistics.maximumLatencyUs.load()
        << " us\n"
        << "Elapsed time:      " << elapsed << " seconds\n";
}
