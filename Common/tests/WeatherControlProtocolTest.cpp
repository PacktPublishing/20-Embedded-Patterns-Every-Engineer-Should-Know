// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include <cassert>
#include <cstddef>

#include "MutableByteView.h"
#include "WeatherControlProtocol.h"

int main()
{
    std::byte storage[16]{};

    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(storage, sizeof(storage)),
        Endianness::Little);

    const weather::SetReportingIntervalRequest request{
        .intervalMilliseconds = 5000u
    };

    weather::writeSetReportingIntervalRequest(writer, request);
    assert(writer.ok());
    assert(writer.bytesWritten() == sizeof(std::uint32_t));

    pbook::BinaryReadStream reader(
        pbook::ImmutableByteView(storage, writer.bytesWritten()),
        Endianness::Little);

    weather::SetReportingIntervalRequest decoded{};
    weather::readSetReportingIntervalRequest(reader, decoded);
    assert(reader.ok());
    assert(decoded.intervalMilliseconds == request.intervalMilliseconds);

    assert(weather::isKnownNackReason(weather::NackReason::Busy));
    assert(weather::isKnownNackReason(weather::NackReason::InvalidValue));
    assert(weather::isKnownNackReason(weather::NackReason::UnsupportedOperation));
    assert(!weather::isKnownNackReason(static_cast<weather::NackReason>(0xFFu)));

    return 0;
}
