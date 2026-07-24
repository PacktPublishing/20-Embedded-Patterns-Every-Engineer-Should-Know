// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>

namespace filtering
{

class ExponentialFilter
{
public:
    explicit ExponentialFilter(double alpha)
        : alpha_{alpha}
    {
        if(!std::isfinite(alpha_) || alpha_ <= 0.0 || alpha_ > 1.0)
        {
            throw std::invalid_argument(
                "exponential alpha must be greater than zero "
                "and no greater than one");
        }
    }

    double update(double sample) noexcept
    {
        if(!current_value_)
        {
            current_value_ = sample;
        }
        else
        {
            current_value_ =
                *current_value_ + alpha_ * (sample - *current_value_);
        }

        return *current_value_;
    }

    [[nodiscard]] std::optional<double> value() const noexcept
    {
        return current_value_;
    }

    [[nodiscard]] double alpha() const noexcept
    {
        return alpha_;
    }

    void reset() noexcept
    {
        current_value_.reset();
    }

private:
    double alpha_;
    std::optional<double> current_value_;
};

} // namespace filtering
