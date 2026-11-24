#pragma once

#include "Ramper.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>

class RampDriver
{
public:
    static constexpr std::size_t zoneCount{8};
    using Commands = std::array<double, zoneCount>;
    using Period = std::chrono::milliseconds;

    struct CycleResult
    {
        std::size_t cycle{};
        std::size_t activeRampers{};
        bool complete{};
    };

    static std::optional<RampDriver> create(
        const Commands& startingCommands,
        const Commands& targetCommands,
        double increment,
        Period period) noexcept;

    // Advances every active transition exactly once.
    CycleResult step() noexcept;

    // Optional wall-clock pacing. Tests and replay code need not call this.
    void waitForNextCycle();

    const Commands& commands() const noexcept;
    std::size_t predictedCycles() const noexcept;
    std::size_t cycle() const noexcept;
    std::size_t activeRampers() const noexcept;
    Period period() const noexcept;

private:
    using Rampers = std::array<std::optional<Ramper>, zoneCount>;
    using Clock = std::chrono::steady_clock;

    RampDriver(
        Commands startingCommands,
        Rampers rampers,
        Period period,
        std::size_t predictedCycles) noexcept;

    Commands commands_;
    Rampers rampers_;
    const Period period_;
    const std::size_t predictedCycles_;
    std::size_t cycle_{0};
    std::size_t activeRampers_{0};
    Clock::time_point nextCycle_{};
    bool clockStarted_{false};
};

