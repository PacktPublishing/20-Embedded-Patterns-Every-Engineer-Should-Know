#pragma once
#include <cstdint>
#include <random>
#include "Options.h"

namespace nmea_sim
{
class ValueGenerator
{
public:
    ValueGenerator() = default;
    ValueGenerator(GeneratorKind kind,
                   std::uint32_t seed,
                   double constant_value,
                   double sine_amplitude,
                   double sine_hz,
                   double random_walk_step_stddev) noexcept;

    double next(double dt_sec) noexcept;

private:
    double next_constant() noexcept;
    double next_sine(double dt_sec) noexcept;
    double next_random_walk() noexcept;

    GeneratorKind m_kind = GeneratorKind::RandomWalk;
    double m_constant = 0.0;
    double m_phase = 0.0;
    double m_amp = 1.0;
    double m_sine_hz = 0.2;
    double m_value = 0.0;
    double m_step_stddev = 0.05;
    std::mt19937 m_rng{};
    std::normal_distribution<double> m_norm{0.0, 1.0};
};
}
