// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>

#include "BdsCommon.h"
#include "ImmutableByteView.h"
#include "MutableByteView.h"
#include "WeatherObservation.h"
#include "WeatherObservationCodec.h"

namespace
{

constexpr std::size_t WeatherObservationWireSize = 3U * sizeof(double);

void roundTrip(Endianness wireEndianness)
{
    const weather::WeatherObservation original{
        21.25,
        1013.25,
        48.5};

    std::array<std::byte, WeatherObservationWireSize> bytes{};

    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(bytes.data(), bytes.size()),
        wireEndianness);

    weather::writeWeatherObservation(writer, original);

    assert(writer.ok());
    assert(writer.bytesWritten() == bytes.size());

    weather::WeatherObservation decoded{};
    pbook::BinaryReadStream reader(
        pbook::ImmutableByteView(bytes.data(), bytes.size()),
        wireEndianness);

    weather::readWeatherObservation(reader, decoded);

    assert(reader.ok());
    assert(reader.bytesRead() == bytes.size());
    assert(decoded.temperatureCelsius == original.temperatureCelsius);
    assert(decoded.pressureHectopascals == original.pressureHectopascals);
    assert(decoded.relativeHumidityPercent ==
           original.relativeHumidityPercent);
}

void verifyKnownLittleEndianBytes()
{
    const weather::WeatherObservation observation{1.0, 2.0, 0.5};

    std::array<std::byte, WeatherObservationWireSize> actual{};
    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(actual.data(), actual.size()),
        Endianness::Little);

    weather::writeWeatherObservation(writer, observation);
    assert(writer.ok());

    const std::array<std::byte, WeatherObservationWireSize> expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0xF0}, std::byte{0x3F},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x40},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0xE0}, std::byte{0x3F}};

    assert(actual == expected);
}

void rejectShortBuffer()
{
    std::array<std::byte, WeatherObservationWireSize - 1U> bytes{};
    const weather::WeatherObservation observation{21.25, 1013.25, 48.5};

    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(bytes.data(), bytes.size()),
        Endianness::Little);

    weather::writeWeatherObservation(writer, observation);

    assert(!writer.ok());
}

} // namespace

int main()
{
    roundTrip(Endianness::Little);
    roundTrip(Endianness::Big);
    verifyKnownLittleEndianBytes();
    rejectShortBuffer();

    std::cout << "All simple weather codec tests passed.\n";
    return 0;
}
