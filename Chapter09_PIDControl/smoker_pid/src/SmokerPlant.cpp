#include "SmokerPlant.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

SmokerPlant::SmokerPlant(Parameters parameters)
    : parameters_{parameters}, temperature_{parameters.initialTemperature}
{
    if (!std::isfinite(parameters_.ambientTemperature) ||
        !std::isfinite(parameters_.initialTemperature) ||
        parameters_.heatingCoefficient < 0.0 ||
        parameters_.heatLossCoefficient < 0.0)
    {
        throw std::invalid_argument{"invalid smoker plant parameters"};
    }
}

void SmokerPlant::update(double fanCommand, double deltaTime)
{
    if (!std::isfinite(fanCommand) || !std::isfinite(deltaTime) || deltaTime <= 0.0)
    {
        throw std::invalid_argument{"fan command and delta time must be finite; delta time must be positive"};
    }

    const double boundedFan = std::clamp(fanCommand, 0.0, 1.0);
    const double heating = parameters_.heatingCoefficient * boundedFan;
    const double heatLoss =
        parameters_.heatLossCoefficient * (temperature_ - parameters_.ambientTemperature);
    temperature_ += (heating - heatLoss) * deltaTime;
}

void SmokerPlant::applyTemperatureDrop(double degrees)
{
    if (!std::isfinite(degrees) || degrees < 0.0)
    {
        throw std::invalid_argument{"temperature drop must be finite and nonnegative"};
    }
    temperature_ = std::max(parameters_.ambientTemperature, temperature_ - degrees);
}

double SmokerPlant::temperature() const noexcept
{
    return temperature_;
}

void SmokerPlant::reset() noexcept
{
    temperature_ = parameters_.initialTemperature;
}
