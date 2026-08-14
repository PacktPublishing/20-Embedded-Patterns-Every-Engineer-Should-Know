// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <cstdint>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"

namespace weather
{

//------------------------------------------------------------------------------
// Weather Device request/response messages used by the control service.
//
// The BDS header carries the transaction identifier and message flags. The
// application payload contains only data specific to the request or response.
//------------------------------------------------------------------------------

inline constexpr std::uint16_t WeatherControlServiceId = 1u;

enum class ControlMessageType : std::uint16_t
{
    SetReportingInterval = 1u,
    Ack = 2u,
    Nack = 3u
};

enum class NackReason : std::uint8_t
{
    Busy = 1u,
    InvalidValue = 2u,
    UnsupportedOperation = 3u
};

struct SetReportingIntervalRequest
{
    std::uint32_t intervalMilliseconds{};
};

struct NackResponse
{
    NackReason reason{NackReason::UnsupportedOperation};
};

inline pbook::BinaryWriteStream& writeSetReportingIntervalRequest(
    pbook::BinaryWriteStream& writer,
    const SetReportingIntervalRequest& request) noexcept
{
    return writer.writeUInt32(request.intervalMilliseconds);
}

inline pbook::BinaryReadStream& readSetReportingIntervalRequest(
    pbook::BinaryReadStream& reader,
    SetReportingIntervalRequest& request) noexcept
{
    return reader.readUInt32(request.intervalMilliseconds);
}

inline pbook::BinaryWriteStream& writeNackResponse(
    pbook::BinaryWriteStream& writer,
    const NackResponse& response) noexcept
{
    return writer.writeUInt8(static_cast<std::uint8_t>(response.reason));
}

inline pbook::BinaryReadStream& readNackResponse(
    pbook::BinaryReadStream& reader,
    NackResponse& response) noexcept
{
    std::uint8_t reason{};
    reader.readUInt8(reason);

    if (reader.ok())
    {
        response.reason = static_cast<NackReason>(reason);
    }

    return reader;
}

inline bool isKnownNackReason(NackReason reason) noexcept
{
    switch (reason)
    {
        case NackReason::Busy:
        case NackReason::InvalidValue:
        case NackReason::UnsupportedOperation:
            return true;
    }

    return false;
}

} // namespace weather
