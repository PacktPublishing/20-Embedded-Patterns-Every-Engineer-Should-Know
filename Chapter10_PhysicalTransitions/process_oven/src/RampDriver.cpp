#include "RampDriver.h"

#include <algorithm>
#include <cmath>
#include <thread>

std::optional<RampDriver> RampDriver::create(
    const Commands& startingCommands,
    const Commands& targetCommands,
    double increment,
    Period period) noexcept
{
    if (!std::isfinite(increment) ||
        increment <= 0.0 ||
        period <= Period::zero())
    {
        return std::nullopt;
    }

    Rampers rampers{};
    std::size_t predictedCycles = 0;

    for (std::size_t i = 0; i < zoneCount; ++i)
    {
        auto ramper = Ramper::create(
            startingCommands[i], targetCommands[i], increment);

        if (!ramper)
            return std::nullopt;

        predictedCycles = std::max(
            predictedCycles, ramper->stepsRemaining());

        if (ramper->stepsRemaining() != 0)
            rampers[i] = std::move(*ramper);
    }

    return RampDriver{
        startingCommands, std::move(rampers), period, predictedCycles};
}

RampDriver::RampDriver(
    Commands startingCommands,
    Rampers rampers,
    Period period,
    std::size_t predictedCycles) noexcept
    : commands_{std::move(startingCommands)},
      rampers_{std::move(rampers)},
      period_{period},
      predictedCycles_{predictedCycles}
{
    activeRampers_ = static_cast<std::size_t>(std::count_if(
        rampers_.begin(), rampers_.end(),
        [](const auto& ramper) { return ramper.has_value(); }));
}

RampDriver::CycleResult RampDriver::step() noexcept
{
    if (activeRampers_ == 0)
        return CycleResult{cycle_, 0, true};

    ++cycle_;

    for (std::size_t i = 0; i < zoneCount; ++i)
    {
        auto& ramper = rampers_[i];

        if (!ramper)
            continue;

        const bool complete = ramper->step();
        commands_[i] = ramper->value();

        if (complete)
        {
            ramper.reset();
            --activeRampers_;
        }
    }

    return CycleResult{cycle_, activeRampers_, activeRampers_ == 0};
}

void RampDriver::waitForNextCycle()
{
    if (!clockStarted_)
    {
        nextCycle_ = Clock::now() + period_;
        clockStarted_ = true;
    }
    else
    {
        nextCycle_ += period_;
    }

    std::this_thread::sleep_until(nextCycle_);
}

const RampDriver::Commands& RampDriver::commands() const noexcept
{
    return commands_;
}

std::size_t RampDriver::predictedCycles() const noexcept
{
    return predictedCycles_;
}

std::size_t RampDriver::cycle() const noexcept
{
    return cycle_;
}

std::size_t RampDriver::activeRampers() const noexcept
{
    return activeRampers_;
}

RampDriver::Period RampDriver::period() const noexcept
{
    return period_;
}

