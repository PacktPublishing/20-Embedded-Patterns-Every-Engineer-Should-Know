// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "MeasurementValidator.h"

#include <cmath>
#include <stdexcept>

namespace filtering
{

MeasurementValidator::MeasurementValidator(MeasurementValidatorConfig config)
    : config_{config}
{
    validate_config(config_);
}

ValidationResult MeasurementValidator::validate(
    double time_seconds,
    std::optional<double> measured_value)
{
    if(!measured_value)
    {
        return {.status = MeasurementStatus::Missing};
    }

    const double value = *measured_value;

    if(!std::isfinite(value))
    {
        return {.status = MeasurementStatus::NonFinite};
    }

    if(value < config_.minimum_value || value > config_.maximum_value)
    {
        return {.status = MeasurementStatus::OutOfRange};
    }

    if(previous_valid_value_ && previous_valid_time_)
    {
        const double elapsed_seconds = time_seconds - *previous_valid_time_;
        const double allowed_change =
            config_.maximum_rate_per_second * elapsed_seconds;
        const double actual_change = std::abs(value - *previous_valid_value_);

        if(actual_change > allowed_change)
        {
            return {.status = MeasurementStatus::ImplausibleRateOfChange};
        }
    }

    previous_valid_value_ = value;
    previous_valid_time_ = time_seconds;

    return {
        .status = MeasurementStatus::Valid,
        .accepted_value = value,
    };
}

void MeasurementValidator::reset() noexcept
{
    previous_valid_value_.reset();
    previous_valid_time_.reset();
}

const MeasurementValidatorConfig& MeasurementValidator::config() const noexcept
{
    return config_;
}

std::optional<double> MeasurementValidator::previous_valid_value() const noexcept
{
    return previous_valid_value_;
}

std::optional<double> MeasurementValidator::previous_valid_time() const noexcept
{
    return previous_valid_time_;
}

std::string_view to_string(MeasurementStatus status) noexcept
{
    switch(status)
    {
    case MeasurementStatus::Valid:
        return "valid";
    case MeasurementStatus::Missing:
        return "missing";
    case MeasurementStatus::NonFinite:
        return "non_finite";
    case MeasurementStatus::OutOfRange:
        return "out_of_range";
    case MeasurementStatus::ImplausibleRateOfChange:
        return "implausible_rate_of_change";
    }

    return "unknown";
}

void validate_config(const MeasurementValidatorConfig& config)
{
    if(!std::isfinite(config.minimum_value) ||
       !std::isfinite(config.maximum_value))
    {
        throw std::invalid_argument(
            "measurement limits must be finite");
    }

    if(config.minimum_value >= config.maximum_value)
    {
        throw std::invalid_argument(
            "minimum value must be less than maximum value");
    }

    if(!std::isfinite(config.maximum_rate_per_second) ||
       config.maximum_rate_per_second < 0.0)
    {
        throw std::invalid_argument(
            "maximum rate per second must be finite and non-negative");
    }
}

} // namespace filtering
