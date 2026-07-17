// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace measurement
{

enum class SignalProfile
{
    Constant,
    Ramp,
    Triangle,
    Step,
};

enum class MeasurementEvent
{
    Normal,
    Spike,
    Missing,
};

struct GeneratorConfig
{
    std::size_t count{140};
    double minimum{22.0};
    double maximum{30.0};
    SignalProfile profile{SignalProfile::Triangle};
    double sample_rate_hz{1.0};
    double noise_stddev{0.25};
    double spike_frequency{0.02};
    double missing_frequency{0.01};
    double spike_minimum{20.0};
    double spike_maximum{30.0};
    std::uint32_t seed{12345};
};

struct GeneratedMeasurement
{
    std::size_t index{};
    double time_seconds{};
    double true_value{};
    std::optional<double> measured_value{};
    MeasurementEvent event{MeasurementEvent::Normal};
};

struct GenerationSummary
{
    std::size_t normal_count{};
    std::size_t spike_count{};
    std::size_t missing_count{};
};

class MeasurementGenerator
{
public:
    explicit MeasurementGenerator(GeneratorConfig config);

    [[nodiscard]] const GeneratorConfig& config() const noexcept;
    [[nodiscard]] std::vector<GeneratedMeasurement> generate();

private:
    [[nodiscard]] double generate_true_value(std::size_t index) const;
    [[nodiscard]] double generate_spike_offset();

    GeneratorConfig config_;
    std::mt19937 random_engine_;
    std::normal_distribution<double> noise_distribution_;
    std::uniform_real_distribution<double> event_distribution_{0.0, 1.0};
    std::uniform_real_distribution<double> spike_magnitude_distribution_;
    std::bernoulli_distribution spike_sign_distribution_{0.5};
};

[[nodiscard]] GenerationSummary summarize(
    const std::vector<GeneratedMeasurement>& measurements) noexcept;

[[nodiscard]] std::string_view to_string(SignalProfile profile) noexcept;
[[nodiscard]] std::string_view to_string(MeasurementEvent event) noexcept;
[[nodiscard]] std::optional<SignalProfile> parse_signal_profile(
    std::string_view text) noexcept;

void validate(const GeneratorConfig& config);

} // namespace measurement
