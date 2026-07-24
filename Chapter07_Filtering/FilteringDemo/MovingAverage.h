// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace filtering
{

class MovingAverage
{
public:
    explicit MovingAverage(std::size_t window_size)
        : samples_(window_size, 0.0)
    {
        if(window_size == 0)
        {
            throw std::invalid_argument(
                "moving-average window size must be greater than zero");
        }
    }

    double update(double sample) noexcept
    {
        if(sample_count_ < samples_.size())
        {
            samples_[next_index_] = sample;
            sum_ += sample;
            ++sample_count_;
        }
        else
        {
            sum_ -= samples_[next_index_];
            samples_[next_index_] = sample;
            sum_ += sample;
        }

        next_index_ = (next_index_ + 1) % samples_.size();
        current_value_ = sum_ / static_cast<double>(sample_count_);
        return *current_value_;
    }

    [[nodiscard]] std::optional<double> value() const noexcept
    {
        return current_value_;
    }

    [[nodiscard]] std::size_t window_size() const noexcept
    {
        return samples_.size();
    }

    [[nodiscard]] std::size_t sample_count() const noexcept
    {
        return sample_count_;
    }

    void reset() noexcept
    {
        next_index_ = 0;
        sample_count_ = 0;
        sum_ = 0.0;
        current_value_.reset();
    }

private:
    std::vector<double> samples_;
    std::size_t next_index_{0};
    std::size_t sample_count_{0};
    double sum_{0.0};
    std::optional<double> current_value_;
};

} // namespace filtering
