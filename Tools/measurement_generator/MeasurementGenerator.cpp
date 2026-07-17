// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "MeasurementGenerator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace measurement
{
namespace
{

[[nodiscard]] double normalized_position(std::size_t index, std::size_t count)
{
    if(count <= 1)
    {
        return 0.0;
    }

    return static_cast<double>(index) / static_cast<double>(count - 1);
}

void validate_frequency(double value, std::string_view name)
{
    if(!std::isfinite(value) || value < 0.0 || value > 1.0)
    {
        throw std::invalid_argument(std::string{name} +
                                    " must be between 0.0 and 1.0");
    }
}

} // namespace

MeasurementGenerator::MeasurementGenerator(GeneratorConfig config)
    : config_{config},
      random_engine_{config.seed},
      noise_distribution_{0.0, config.noise_stddev},
      spike_magnitude_distribution_{config.spike_minimum,
                                    config.spike_maximum}
{
    validate(config_);
}

const GeneratorConfig& MeasurementGenerator::config() const noexcept
{
    return config_;
}

std::vector<GeneratedMeasurement> MeasurementGenerator::generate()
{
    random_engine_.seed(config_.seed);
    noise_distribution_.reset();
    event_distribution_.reset();
    spike_magnitude_distribution_.reset();
    spike_sign_distribution_.reset();

    std::vector<GeneratedMeasurement> measurements;
    measurements.reserve(config_.count);

    for(std::size_t index = 0; index < config_.count; ++index)
    {
        const double true_value = generate_true_value(index);
        const double time_seconds =
            static_cast<double>(index) / config_.sample_rate_hz;

        GeneratedMeasurement measurement{
            .index = index,
            .time_seconds = time_seconds,
            .true_value = true_value,
            .measured_value = std::nullopt,
            .event = MeasurementEvent::Missing,
        };

        const double event_sample = event_distribution_(random_engine_);

        if(event_sample < config_.missing_frequency)
        {
            measurements.push_back(measurement);
            continue;
        }

        if(event_sample <
           config_.missing_frequency + config_.spike_frequency)
        {
            measurement.measured_value = true_value + generate_spike_offset();
            measurement.event = MeasurementEvent::Spike;
            measurements.push_back(measurement);
            continue;
        }

        measurement.measured_value =
            true_value + noise_distribution_(random_engine_);
        measurement.event = MeasurementEvent::Normal;
        measurements.push_back(measurement);
    }

    return measurements;
}

double MeasurementGenerator::generate_true_value(std::size_t index) const
{
    const double midpoint = (config_.minimum + config_.maximum) / 2.0;
    const double span = config_.maximum - config_.minimum;
    const double position = normalized_position(index, config_.count);

    switch(config_.profile)
    {
    case SignalProfile::Constant:
        return midpoint;

    case SignalProfile::Ramp:
        return config_.minimum + span * position;

    case SignalProfile::Triangle:
    {
        const double triangle_position =
            position <= 0.5 ? position * 2.0 : (1.0 - position) * 2.0;
        return config_.minimum + span * triangle_position;
    }

    case SignalProfile::Step:
        return position < 0.5 ? config_.minimum : config_.maximum;
    }

    throw std::logic_error("Unsupported signal profile");
}

double MeasurementGenerator::generate_spike_offset()
{
    const double magnitude = spike_magnitude_distribution_(random_engine_);
    return spike_sign_distribution_(random_engine_) ? magnitude : -magnitude;
}

GenerationSummary summarize(
    const std::vector<GeneratedMeasurement>& measurements) noexcept
{
    GenerationSummary summary{};

    for(const auto& measurement : measurements)
    {
        switch(measurement.event)
        {
        case MeasurementEvent::Normal:
            ++summary.normal_count;
            break;
        case MeasurementEvent::Spike:
            ++summary.spike_count;
            break;
        case MeasurementEvent::Missing:
            ++summary.missing_count;
            break;
        }
    }

    return summary;
}

std::string_view to_string(SignalProfile profile) noexcept
{
    switch(profile)
    {
    case SignalProfile::Constant:
        return "constant";
    case SignalProfile::Ramp:
        return "ramp";
    case SignalProfile::Triangle:
        return "triangle";
    case SignalProfile::Step:
        return "step";
    }

    return "unknown";
}

std::string_view to_string(MeasurementEvent event) noexcept
{
    switch(event)
    {
    case MeasurementEvent::Normal:
        return "normal";
    case MeasurementEvent::Spike:
        return "spike";
    case MeasurementEvent::Missing:
        return "missing";
    }

    return "unknown";
}

std::optional<SignalProfile> parse_signal_profile(std::string_view text) noexcept
{
    if(text == "constant")
    {
        return SignalProfile::Constant;
    }
    if(text == "ramp")
    {
        return SignalProfile::Ramp;
    }
    if(text == "triangle")
    {
        return SignalProfile::Triangle;
    }
    if(text == "step")
    {
        return SignalProfile::Step;
    }

    return std::nullopt;
}

void validate(const GeneratorConfig& config)
{
    if(config.count == 0)
    {
        throw std::invalid_argument("count must be greater than zero");
    }
    if(!std::isfinite(config.minimum) || !std::isfinite(config.maximum))
    {
        throw std::invalid_argument("minimum and maximum must be finite");
    }
    if(config.minimum >= config.maximum)
    {
        throw std::invalid_argument("minimum must be less than maximum");
    }
    if(!std::isfinite(config.sample_rate_hz) || config.sample_rate_hz <= 0.0)
    {
        throw std::invalid_argument("sample rate must be greater than zero");
    }
    if(!std::isfinite(config.noise_stddev) || config.noise_stddev < 0.0)
    {
        throw std::invalid_argument(
            "noise standard deviation must not be negative");
    }

    validate_frequency(config.spike_frequency, "spike frequency");
    validate_frequency(config.missing_frequency, "missing frequency");

    if(config.spike_frequency + config.missing_frequency > 1.0)
    {
        throw std::invalid_argument(
            "spike frequency plus missing frequency must not exceed 1.0");
    }
    if(!std::isfinite(config.spike_minimum) ||
       !std::isfinite(config.spike_maximum) || config.spike_minimum < 0.0 ||
       config.spike_maximum < 0.0)
    {
        throw std::invalid_argument(
            "spike magnitudes must be finite and non-negative");
    }
    if(config.spike_minimum > config.spike_maximum)
    {
        throw std::invalid_argument(
            "spike minimum must not exceed spike maximum");
    }
}

} // namespace measurement
