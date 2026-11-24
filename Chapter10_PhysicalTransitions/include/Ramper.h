#pragma once

#include <cstddef>
#include <optional>

class Ramper
{
public:
    static std::optional<Ramper> create(
        double start,
        double target,
        double increment) noexcept;

    // Advances one bounded increment and reports whether the target was reached.
    bool step() noexcept;

    double value() const noexcept;
    double target() const noexcept;
    double increment() const noexcept;
    std::size_t stepsRemaining() const noexcept;

private:
    Ramper(double start, double target, double increment) noexcept;

    double current_;
    // There are deliberately no mutators for the transition definition.
    // Non-const storage keeps the value type assignable inside std::optional.
    double target_;
    double increment_;
};
