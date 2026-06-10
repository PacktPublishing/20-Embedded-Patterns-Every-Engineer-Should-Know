// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "NMEAParseStatus.h"

namespace weather
{
    // Builds an NMEA sentence by appending comma-separated fields and a checksum.
    //
    // - Starts with "$" + identifier (exactly 5 chars: talker(2)+type(3))
    // - Each write call appends one field (including empty fields)
    // - finalize() appends "*HH" and optional CRLF
    class InsertionStream
    {
    public:
        explicit InsertionStream(std::string_view identifier);

        NmeaParseStatus status() const noexcept;

        NmeaParseStatus writeString(std::string_view v);
        NmeaParseStatus writeChar(char v);
        NmeaParseStatus writeInt(int v);
        NmeaParseStatus writeDouble(double v);

        NmeaParseStatus writeOptionalString(const std::optional<std::string_view>& v);
        NmeaParseStatus writeOptionalChar(const std::optional<char>& v);
        NmeaParseStatus writeOptionalInt(const std::optional<int>& v);
        NmeaParseStatus writeOptionalDouble(const std::optional<double>& v);

        NmeaParseStatus writeEmpty();

        NmeaParseStatus finalize(bool appendCRLF = true);

        bool finalized() const noexcept;
        const std::string& sentence() const noexcept;

    private:
        void appendFieldPrefix();
        void setError(NmeaParseErrorCode code) noexcept;

        static std::uint8_t computeXor(std::string_view betweenDollarAndStar) noexcept;
        static void appendChecksum(std::string& out, std::uint8_t checksum);

        NmeaParseStatus m_status{};
        std::string m_sentence;
        bool m_finalized = false;
    };
} // namespace nmea
