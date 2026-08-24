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
    WindSpeed = 1u
};

struct WindSpeed
{
    std::uint32_t sequence{};
    std::int64_t eventTimeNs{};
    double metersPerSecond{};
};

struct WindDirection
{
    std::uint32_t sequence{};
    std::int64_t eventTimeNs{};
    double degrees{};
};

using Measurement = std::variant<WindSpeed, WindDirection>;

inline std::int64_t synchronizedTimeNowNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline pbook::BinaryWriteStream& writeWindSpeed(
    pbook::BinaryWriteStream& writer,
    const WindSpeed& measurement) noexcept
{
    return writer
        .writeUInt32(measurement.sequence)
        .writeInt64(measurement.eventTimeNs)
        .writeDouble(measurement.metersPerSecond);
}

inline pbook::BinaryReadStream& readWindSpeed(
    pbook::BinaryReadStream& reader,
    WindSpeed& measurement) noexcept
{
    return reader
        .readUInt32(measurement.sequence)
        .readInt64(measurement.eventTimeNs)
        .readDouble(measurement.metersPerSecond);
}

} // namespace ch14
