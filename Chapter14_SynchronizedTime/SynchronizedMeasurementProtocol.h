// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"

#include <chrono>
#include <cstdint>
#include <variant>

namespace ch14
{

inline constexpr std::uint16_t SynchronizedMeasurementServiceId = 14u;

enum class MeasurementMessageType : std::uint16_t
{
    Temperature = 1u
};

struct Temperature
{
    std::uint32_t sequence{};
    std::int64_t eventTimeNs{};
    double degreesCelsius{};
};

struct Pressure
{
    std::uint32_t sequence{};
    std::int64_t eventTimeNs{};
    double hectopascals{};
};

using Measurement = std::variant<Temperature, Pressure>;

inline std::int64_t synchronizedTimeNowNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline pbook::BinaryWriteStream& writeTemperature(
    pbook::BinaryWriteStream& writer,
    const Temperature& measurement) noexcept
{
    return writer
        .writeUInt32(measurement.sequence)
        .writeInt64(measurement.eventTimeNs)
        .writeDouble(measurement.degreesCelsius);
}

inline pbook::BinaryReadStream& readTemperature(
    pbook::BinaryReadStream& reader,
    Temperature& measurement) noexcept
{
    return reader
        .readUInt32(measurement.sequence)
        .readInt64(measurement.eventTimeNs)
        .readDouble(measurement.degreesCelsius);
}

} // namespace ch14
