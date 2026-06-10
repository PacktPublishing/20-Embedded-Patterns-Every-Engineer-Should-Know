// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "NMEAParseStatus.h"
#include "NMEATokenizer.h"

namespace weather
{
    // Sequential typed extraction over tokenized NMEA data fields.
    //
    // Required reads fail if:
    //  - the field is missing (sentence has too few fields), OR
    //  - the field exists but is empty.
    //
    // Optional reads fail only if the field is missing. If the field is empty,
    // the optional is cleared and NMEAParseStatus::Ok is returned.
    class ExtractionStream
    {
    public:
        explicit ExtractionStream(const Tokenizer& tokens);

        std::size_t index() const noexcept;
        bool atEnd() const noexcept;
        NmeaParseStatus lastStatus() const noexcept;

        NmeaParseStatus skipField();

        NmeaParseStatus readString(std::string_view& out);
        NmeaParseStatus readChar(char& out);
        NmeaParseStatus readInt(int& out);
        NmeaParseStatus readDouble(double& out);

        NmeaParseStatus readOptionalString(std::optional<std::string_view>& out);
        NmeaParseStatus readOptionalChar(std::optional<char>& out);
        NmeaParseStatus readOptionalInt(std::optional<int>& out);
        NmeaParseStatus readOptionalDouble(std::optional<double>& out);

    private:
        NmeaParseStatus requireField(std::string_view& out);
        NmeaParseStatus optionalField(std::optional<std::string_view>& out);

        static NmeaParseStatus parseInt(std::string_view s, int& out, std::size_t fieldIndex, std::string_view ctx) noexcept;
        static NmeaParseStatus parseDouble(std::string_view s, double& out, std::size_t fieldIndex, std::string_view ctx) noexcept;

        const Tokenizer& m_tokens;
        Tokenizer::FieldIterator m_it;
        Tokenizer::FieldIterator m_end;
        std::size_t m_index = 0;
        NmeaParseStatus m_last = NmeaParseStatus{};
    };
} // namespace nmea
