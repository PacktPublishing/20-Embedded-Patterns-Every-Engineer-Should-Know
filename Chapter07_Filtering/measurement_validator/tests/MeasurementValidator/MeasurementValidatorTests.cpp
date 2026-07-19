// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "MeasurementValidator.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace
{

int failures = 0;

void expect(bool condition, std::string_view message)
{
    if(!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

filtering::MeasurementValidator make_validator()
{
    return filtering::MeasurementValidator{{
        .minimum_value = 0.0,
        .maximum_value = 50.0,
        .maximum_rate_per_second = 2.0,
    }};
}

void test_valid_measurement()
{
    auto validator = make_validator();
    const auto result = validator.validate(0.0, 22.0);

    expect(result.is_valid(), "first finite in-range value is valid");
    expect(result.measured_value == std::optional<double>{22.0},
           "valid result retains measured value");
}

void test_missing_measurement()
{
    auto validator = make_validator();
    const auto result = validator.validate(0.0, std::nullopt);

    expect(result.status == filtering::MeasurementStatus::Missing,
           "missing value has Missing status");
    expect(!result.measured_value,
           "missing result contains no measured value");
}

void test_non_finite_measurement_is_retained()
{
    auto validator = make_validator();
    const double value = std::numeric_limits<double>::infinity();
    const auto result = validator.validate(0.0, value);

    expect(result.status == filtering::MeasurementStatus::NonFinite,
           "infinite value has NonFinite status");
    expect(result.measured_value && std::isinf(*result.measured_value),
           "non-finite value is retained for observability");
}

void test_out_of_range_measurement_is_retained()
{
    auto validator = make_validator();
    const auto result = validator.validate(0.0, 75.0);

    expect(result.status == filtering::MeasurementStatus::OutOfRange,
           "out-of-range value has OutOfRange status");
    expect(result.measured_value == std::optional<double>{75.0},
           "out-of-range value is retained for observability");
}

void test_implausible_change_is_retained_and_not_baseline()
{
    auto validator = make_validator();

    expect(validator.validate(0.0, 20.0).is_valid(),
           "initial value is valid");

    const auto rejected = validator.validate(1.0, 30.0);
    expect(rejected.status ==
               filtering::MeasurementStatus::ImplausibleRateOfChange,
           "implausible change is rejected");
    expect(rejected.measured_value == std::optional<double>{30.0},
           "rejected spike is retained for observability");

    const auto next = validator.validate(2.0, 21.0);
    expect(next.is_valid(),
           "rejected value does not become the comparison baseline");
}

void test_elapsed_time_allows_change_after_missing_sample()
{
    auto validator = make_validator();

    expect(validator.validate(0.0, 20.0).is_valid(),
           "initial value is valid");
    expect(validator.validate(1.0, std::nullopt).status ==
               filtering::MeasurementStatus::Missing,
           "middle sample is missing");
    expect(validator.validate(2.0, 24.0).is_valid(),
           "two-second gap permits four units of change");
}

void test_reset_clears_baseline()
{
    auto validator = make_validator();

    expect(validator.validate(0.0, 20.0).is_valid(),
           "initial value is valid");
    validator.reset();
    expect(validator.validate(1.0, 40.0).is_valid(),
           "first value after reset establishes a new baseline");
}

void test_invalid_configurations_throw()
{
    bool threw = false;

    try
    {
        filtering::MeasurementValidator validator{{
            .minimum_value = 10.0,
            .maximum_value = 10.0,
            .maximum_rate_per_second = 1.0,
        }};
        (void)validator;
    }
    catch(const std::invalid_argument&)
    {
        threw = true;
    }

    expect(threw, "minimum must be less than maximum");
}

} // namespace

int main()
{
    test_valid_measurement();
    test_missing_measurement();
    test_non_finite_measurement_is_retained();
    test_out_of_range_measurement_is_retained();
    test_implausible_change_is_retained_and_not_baseline();
    test_elapsed_time_allows_change_after_missing_sample();
    test_reset_clears_baseline();
    test_invalid_configurations_throw();

    if(failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All MeasurementValidator tests passed\n";
    return EXIT_SUCCESS;
}
