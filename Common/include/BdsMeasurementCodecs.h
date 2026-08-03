// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#pragma once

namespace weather
{

class BinaryReadStream;
class BinaryWriteStream;

//
// Forward declarations
//
struct MeasurementHeaderV1;

struct Temperature;
struct BarometricPressure;
struct Humidity;
struct WindSpeed;
struct WindDirection;
struct Precipitation;
struct Position;

BinaryWriteStream& writeMeasurementHeader(BinaryWriteStream& stream, const MeasurementHeaderV1& header) noexcept;
BinaryReadStream& readMeasurementHeader(BinaryReadStream& stream, MeasurementHeaderV1& header) noexcept;



BinaryWriteStream& writeTemperature(BinaryWriteStream& stream, const Temperature& measurement) noexcept;
BinaryWriteStream& writeBarometricPressure(BinaryWriteStream& stream, const BarometricPressure& measurement) noexcept;
BinaryWriteStream& writeHumidity(BinaryWriteStream& stream, const Humidity& measurement) noexcept;
BinaryWriteStream& writeWindSpeed(BinaryWriteStream& stream, const WindSpeed& measurement) noexcept;
BinaryWriteStream& writeDirection(BinaryWriteStream& stream, const WindDirection& measurement) noexcept;
BinaryWriteStream& writePrecipitation(BinaryWriteStream& stream, const Precipitation& measurement) noexcept;
BinaryWriteStream& writePosition(BinaryWriteStream& stream, const Position& measurement) noexcept;


BinaryReadStream& readTemperature(BinaryReadStream& stream, Temperature& measurement) noexcept;
BinaryReadStream& readBarometricPressure(BinaryReadStream& stream, BarometricPressure& measurement) noexcept;
BinaryReadStream& readHumidity(BinaryReadStream& stream, Humidity& measurement) noexcept;
BinaryReadStream& readWindSpeed(BinaryReadStream& stream, WindSpeed& measurement) noexcept;
BinaryReadStream& readWindDirection(BinaryReadStream& stream, WindDirection& measurement) noexcept;
BinaryReadStream& readPrecipitation(BinaryReadStream& stream, Precipitation& measurement) noexcept;
BinaryReadStream& readPosition(BinaryReadStream& stream, Position& measurement) noexcept;


} // end namespace weather
