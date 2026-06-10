// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace weather
{
    enum class NmeaParseErrorCode : std::uint8_t
    {
        Ok = 0,

        // Framing
        EmptyInput,
        MissingStartDelimiter,
        MissingChecksumDelimiter,
        MissingChecksumValue,
        InvalidChecksumValue,

        // Structure
        MissingIdentifier,
        InvalidIdentifierLength,
        AlreadyFinalized,
        OutputBufferTooSmall,

        // Field access
        FieldMissing,
        FieldEmpty,

        // Conversions
        InvalidInteger,
        IntegerOutOfRange,
        InvalidDouble,
        InvalidChar
    };

    struct NmeaParseStatus
    {
        NmeaParseErrorCode code = NmeaParseErrorCode::Ok;
        std::size_t fieldIndex = 0;
        std::string_view context{};

        bool ok() const noexcept
        {
            return code == NmeaParseErrorCode::Ok;
        }
    };
}

