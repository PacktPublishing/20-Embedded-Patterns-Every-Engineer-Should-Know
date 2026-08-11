// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

namespace weather
{

struct WeatherObservation
{
    double temperatureCelsius{};
    double pressureHectopascals{};
    double relativeHumidityPercent{};
};

} // namespace weather
