// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
#include "WeatherObservation.h"

namespace weather
{

inline pbook::BinaryWriteStream& writeWeatherObservation(
    pbook::BinaryWriteStream& writer,
    const WeatherObservation& observation) noexcept
{
    writer.writeDouble(observation.temperatureCelsius)
        .writeDouble(observation.pressureHectopascals)
        .writeDouble(observation.relativeHumidityPercent);

    return writer;
}

inline pbook::BinaryReadStream& readWeatherObservation(
    pbook::BinaryReadStream& reader,
    WeatherObservation& observation) noexcept
{
    reader.readDouble(observation.temperatureCelsius)
        .readDouble(observation.pressureHectopascals)
        .readDouble(observation.relativeHumidityPercent);

    return reader;
}

} // namespace weather
