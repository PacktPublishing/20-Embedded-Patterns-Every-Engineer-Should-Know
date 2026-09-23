// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace chapter19
{

enum class MeasurementQuality : std::uint8_t
{
    Good,
    Suspect,
    Invalid
};

template<typename T>
struct QualifiedMeasurement
{
    T measurement;
    MeasurementQuality quality{MeasurementQuality::Good};
};

template<typename T>
class RangeFilter
{
public:
    RangeFilter(T lower, T upper)
        : lower_{lower},
          upper_{upper}
    {
    }

    [[nodiscard]]
    bool inRange(T value) const
    {
        return value >= lower_ && value <= upper_;
    }

private:
    T lower_;
    T upper_;
};

template<typename T>
class RateOfChangeChecker
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit RateOfChangeChecker(double maximumRate)
        : maximumRate_{maximumRate}
    {
    }

    [[nodiscard]]
    bool changeNormal(T newValue, TimePoint timestamp)
    {
        if (!previousValue_)
        {
            previousValue_ = newValue;
            previousTimestamp_ = timestamp;
            return true;
        }

        const double elapsedSeconds =
            std::chrono::duration<double>(
                timestamp - *previousTimestamp_).count();

        if (elapsedSeconds <= 0.0)
        {
            return false;
        }

        const double change =
            std::abs(static_cast<double>(newValue) -
                     static_cast<double>(*previousValue_));

        const double rate = change / elapsedSeconds;

        previousValue_ = newValue;
        previousTimestamp_ = timestamp;

        return rate <= maximumRate_;
    }

private:
    double maximumRate_;
    std::optional<T> previousValue_;
    std::optional<TimePoint> previousTimestamp_;
};

template<typename T>
class StuckSensorChecker
{
public:
    explicit StuckSensorChecker(std::size_t stuckThreshold)
        : stuckThreshold_{stuckThreshold}
    {
    }

    [[nodiscard]]
    bool valueNormal(T newValue)
    {
        if (!previousValue_)
        {
            previousValue_ = newValue;
            identicalCount_ = 1;
            return true;
        }

        if (newValue == *previousValue_)
        {
            ++identicalCount_;
        }
        else
        {
            previousValue_ = newValue;
            identicalCount_ = 1;
        }

        return identicalCount_ < stuckThreshold_;
    }

private:
    std::size_t stuckThreshold_;
    std::size_t identicalCount_{0};
    std::optional<T> previousValue_;
};

class StaleSensorChecker
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    explicit StaleSensorChecker(Duration maximumAge)
        : maximumAge_{maximumAge}
    {
    }

    void measurementReceived(TimePoint timestamp)
    {
        lastReceived_ = timestamp;
    }

    [[nodiscard]]
    bool stale(TimePoint now) const
    {
        return lastReceived_ && now - *lastReceived_ > maximumAge_;
    }

private:
    Duration maximumAge_;
    std::optional<TimePoint> lastReceived_;
};

template<typename T, typename Downstream>
class AnomalyDetectionNode
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    AnomalyDetectionNode(
        T lower,
        T upper,
        double maximumRate,
        std::size_t stuckThreshold,
        Duration maximumAge,
        Downstream& downstream)
        : rangeFilter_{lower, upper},
          rateChecker_{maximumRate},
          stuckChecker_{stuckThreshold},
          staleChecker_{maximumAge},
          downstream_{downstream}
    {
    }

    void process(
        QualifiedMeasurement<T> measurement,
        TimePoint arrivalTime)
    {
        if (measurement.quality == MeasurementQuality::Invalid)
        {
            downstream_.process(measurement);
            return;
        }

        if (!rangeFilter_.inRange(measurement.measurement))
        {
            measurement.quality = MeasurementQuality::Invalid;
            downstream_.process(measurement);
            return;
        }

        staleChecker_.measurementReceived(arrivalTime);
        staleReported_ = false;

        if (!rateChecker_.changeNormal(
                measurement.measurement,
                arrivalTime))
        {
            measurement.quality = MeasurementQuality::Suspect;
        }

        if (!stuckChecker_.valueNormal(measurement.measurement))
        {
            measurement.quality = MeasurementQuality::Suspect;
        }

        latestMeasurement_ = measurement;
        downstream_.process(measurement);
    }

    void checkForStaleMeasurement(TimePoint now)
    {
        if (!latestMeasurement_ || staleReported_ ||
            !staleChecker_.stale(now))
        {
            return;
        }

        if (latestMeasurement_->quality == MeasurementQuality::Good)
        {
            latestMeasurement_->quality = MeasurementQuality::Suspect;
        }

        staleReported_ = true;
        downstream_.process(*latestMeasurement_);
    }

private:
    RangeFilter<T> rangeFilter_;
    RateOfChangeChecker<T> rateChecker_;
    StuckSensorChecker<T> stuckChecker_;
    StaleSensorChecker staleChecker_;
    Downstream& downstream_;
    std::optional<QualifiedMeasurement<T>> latestMeasurement_;
    bool staleReported_{false};
};

template<typename T, typename Downstream>
class MeasurementSourceNode
{
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    explicit MeasurementSourceNode(Downstream& downstream)
        : downstream_{downstream}
    {
    }

    void emit(
        T value,
        TimePoint arrivalTime,
        MeasurementQuality quality = MeasurementQuality::Good)
    {
        downstream_.process(
            QualifiedMeasurement<T>{value, quality},
            arrivalTime);
    }

private:
    Downstream& downstream_;
};

} // namespace chapter19
