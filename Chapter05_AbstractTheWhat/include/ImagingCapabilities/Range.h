#pragma once

#include <vector>

namespace ImagingCapabilities
{

template <typename T>
struct RangeLimit
{
    T value{};
    bool inclusive{true};
};

template <typename T>
struct Range
{
    using ValueType = T;

    RangeLimit<T> lower{};
    RangeLimit<T> upper{};

    constexpr Range() = default;

    constexpr explicit Range(T singleValue) :
        lower{singleValue, true},
        upper{singleValue, true}
    {
    }

    constexpr Range(T minValue, T maxValue) :
        lower{minValue, true},
        upper{maxValue, true}
    {
    }

    constexpr Range(RangeLimit<T> lowerLimit, RangeLimit<T> upperLimit) :
        lower{lowerLimit},
        upper{upperLimit}
    {
    }

    constexpr T min() const noexcept { return lower.value; }
    constexpr T max() const noexcept { return upper.value; }

    constexpr bool isDegenerate() const noexcept
    {
        return lower.value == upper.value;
    }
};

template <typename T>
constexpr bool contains(const Range<T>& range, T value) noexcept
{
    const bool aboveLower = range.lower.inclusive ?
        value >= range.lower.value : value > range.lower.value;

    const bool belowUpper = range.upper.inclusive ?
        value <= range.upper.value : value < range.upper.value;

    return aboveLower && belowUpper;
}

template <typename T>
bool containsAny(const std::vector<Range<T>>& ranges, T value)
{
    for (const auto& range : ranges)
    {
        if (contains(range, value))
            return true;
    }

    return false;
}

template <typename T>
bool isDiscrete(const std::vector<Range<T>>& ranges)
{
    for (const auto& range : ranges)
    {
        if (!range.isDegenerate())
            return false;
    }

    return true;
}

template <typename T>
std::vector<T> discreteValues(const std::vector<Range<T>>& ranges)
{
    std::vector<T> values;

    if (!isDiscrete(ranges))
        return values;

    values.reserve(ranges.size());
    for (const auto& range : ranges)
        values.push_back(range.lower.value);

    return values;
}

}
