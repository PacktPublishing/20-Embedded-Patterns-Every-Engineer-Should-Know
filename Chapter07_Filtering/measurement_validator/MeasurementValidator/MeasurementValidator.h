// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <optional>
#include <string_view>

namespace filtering
{

enum class MeasurementStatus
{
    Valid,
    Missing,
    NonFinite,
    OutOfRange,
    ImplausibleRateOfChange,
};

struct MeasurementValidatorConfig
{
    double minimum_value{0.0};
    double maximum_value{50.0};
    double maximum_rate_per_second{2.0};
};

struct ValidationResult
{
    MeasurementStatus status{MeasurementStatus::Missing};
    std::optional<double> measured_value{};

    [[nodiscard]] bool is_valid() const noexcept
    {
        return status == MeasurementStatus::Valid;
    }
};

class MeasurementValidator
{
public:
    explicit MeasurementValidator(MeasurementValidatorConfig config);

    // Precondition: time_seconds values are finite and strictly increasing.
    [[nodiscard]] ValidationResult validate(
        double time_seconds,
        std::optional<double> measured_value);

    void reset() noexcept;

private:
    MeasurementValidatorConfig config_;
    std::optional<double> previous_valid_value_;
    std::optional<double> previous_valid_time_;
};

[[nodiscard]] std::string_view to_string(
    MeasurementStatus status) noexcept;

void validate_config(const MeasurementValidatorConfig& config);

} // namespace filtering
