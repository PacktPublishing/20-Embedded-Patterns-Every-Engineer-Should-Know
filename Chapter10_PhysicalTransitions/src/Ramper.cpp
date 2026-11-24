#include "Ramper.h"

#include <algorithm>
#include <cmath>
#include <limits>

std::optional<Ramper> Ramper::create(
    double start,
    double target,
    double increment) noexcept
{
    if (!std::isfinite(start) ||
        !std::isfinite(target) ||
        !std::isfinite(increment) ||
        increment <= 0.0)
    {
        return std::nullopt;
    }

    const double distance = std::abs(target - start);
    const double stepCount = std::ceil(distance / increment);

    if (!std::isfinite(stepCount) ||
        stepCount > static_cast<double>(
            std::numeric_limits<std::size_t>::max()))
    {
        return std::nullopt;
    }

    return Ramper{start, target, increment};
}

Ramper::Ramper(double start, double target, double increment) noexcept
    : current_{start},
      target_{target},
      increment_{increment}
{
}

bool Ramper::step() noexcept
{
    if (current_ < target_)
        current_ = std::min(current_ + increment_, target_);
    else if (current_ > target_)
        current_ = std::max(current_ - increment_, target_);

    return current_ == target_;
}

double Ramper::value() const noexcept
{
    return current_;
}

double Ramper::target() const noexcept
{
    return target_;
}

double Ramper::increment() const noexcept
{
    return increment_;
}

std::size_t Ramper::stepsRemaining() const noexcept
{
    const double distance = std::abs(target_ - current_);
    return static_cast<std::size_t>(std::ceil(distance / increment_));
}

