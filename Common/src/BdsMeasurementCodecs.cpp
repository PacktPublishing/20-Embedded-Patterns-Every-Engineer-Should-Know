// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#include <cstdint>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"

#include "MeasurementTypes.h"
#include "MeasurementHeaderV1.h"
#include "BdsMeasurementCodecs.h"

namespace weather
{

BinaryWriteStream& writeMeasurementHeader(BinaryWriteStream& stream,
                                          const MeasurementHeaderV1& header) noexcept
{
    // MeasurementHeaderV1 payload prefix layout:
    //   uint64 rxTime
    //   uint64 eventTime
    //   uint16 kind
    //   uint16 source
    //   uint32 flags

    stream.writeUInt64(header.rxTime);
    stream.writeUInt64(header.eventTime);
    stream.writeUInt16(static_cast<std::uint16_t>(header.kind));
    stream.writeUInt16(static_cast<std::uint16_t>(header.source));
    stream.writeUInt32(header.flags);

    return stream;
}

BinaryReadStream& readMeasurementHeader(BinaryReadStream& stream,
                                        MeasurementHeaderV1& header) noexcept
{
    std::uint16_t kindRaw = 0;
    std::uint16_t sourceRaw = 0;

    stream.readUInt64(header.rxTime);
    stream.readUInt64(header.eventTime);
    stream.readUInt16(kindRaw);
    stream.readUInt16(sourceRaw);
    stream.readUInt32(header.flags);

    // If you want strict validation, range-check kindRaw/sourceRaw here.
    header.kind   = static_cast<MeasurementKind>(kindRaw);
    header.source = static_cast<SourceId>(sourceRaw);

    return stream;
}

BinaryWriteStream& writeTemperature(BinaryWriteStream& stream,
                                    const Temperature& measurement) noexcept
{
    // Temperature payload:
    //   double value
    stream.writeDouble(measurement.value);
    return stream;
}

BinaryReadStream& readTemperature(BinaryReadStream& stream,
                                  Temperature& measurement) noexcept
{
    stream.readDouble(measurement.value);
    return stream;
}

BinaryWriteStream& writeBarometricPressure(BinaryWriteStream& stream,
                                           const BarometricPressure& measurement) noexcept
{
    // BarometricPressure payload:
    //   double value
    stream.writeDouble(measurement.value);
    return stream;
}

BinaryReadStream& readBarometricPressure(BinaryReadStream& stream,
                                         BarometricPressure& measurement) noexcept
{
    stream.readDouble(measurement.value);
    return stream;
}

BinaryWriteStream& writeHumidity(BinaryWriteStream& stream,
                                 const Humidity& measurement) noexcept
{
    // Humidity payload:
    //   double value
    stream.writeDouble(measurement.value);
    return stream;
}

BinaryReadStream& readHumidity(BinaryReadStream& stream,
                               Humidity& measurement) noexcept
{
    stream.readDouble(measurement.value);
    return stream;
}

BinaryWriteStream& writeWindSpeed(BinaryWriteStream& stream,
                                  const WindSpeed& measurement) noexcept
{
    // WindSpeed payload:
    //   double value
    stream.writeDouble(measurement.value);
    return stream;
}

BinaryReadStream& readWindSpeed(BinaryReadStream& stream,
                                WindSpeed& measurement) noexcept
{
    stream.readDouble(measurement.value);
    return stream;
}

BinaryWriteStream& writeWindDirection(BinaryWriteStream& stream,
                                      const WindDirection& measurement) noexcept
{
    // WindDirection payload:
    //   double value
    stream.writeDouble(measurement.value);
    return stream;
}

BinaryReadStream& readWindDirection(BinaryReadStream& stream,
                                    WindDirection& measurement) noexcept
{
    stream.readDouble(measurement.value);
    return stream;
}

BinaryWriteStream& writePrecipitation(BinaryWriteStream& stream,
                                      const Precipitation& measurement) noexcept
{
    // Precipitation payload:
    //   double value
    stream.writeDouble(measurement.value);
    return stream;
}

BinaryReadStream& readPrecipitation(BinaryReadStream& stream,
                                    Precipitation& measurement) noexcept
{
    stream.readDouble(measurement.value);
    return stream;
}

BinaryWriteStream& writePosition(BinaryWriteStream& stream,
                                 const Position& measurement) noexcept
{
    // Position payload:
    //   double lat
    //   double lon
    //   double alt
    stream.writeDouble(measurement.lat);
    stream.writeDouble(measurement.lon);
    stream.writeDouble(measurement.alt);
    return stream;
}

BinaryReadStream& readPosition(BinaryReadStream& stream,
                               Position& measurement) noexcept
{
    stream.readDouble(measurement.lat);
    stream.readDouble(measurement.lon);
    stream.readDouble(measurement.alt);
    return stream;
}

} // namespace weather
