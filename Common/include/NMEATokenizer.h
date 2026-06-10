// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "NMEAParseStatus.h"

namespace weather
{
    // Tokenizes a single NMEA sentence string.
    //
    // - Operates on std::string_view and returns views into the original buffer.
    // - Preserves empty fields (`,,`) as empty string_view tokens.
    // - Exposes the 5-character identifier separately (talker + sentence type).
    //
    // Iterator-only: fields are produced on demand via a forward iterator.
    class Tokenizer
    {
    public:
        class FieldIterator
        {
        public:
            using value_type = std::string_view;

            FieldIterator() = default;

            std::string_view operator*() const noexcept;
            FieldIterator& operator++() noexcept;

            bool operator==(const FieldIterator& rhs) const noexcept;
            bool operator!=(const FieldIterator& rhs) const noexcept;

        private:
            friend class Tokenizer;

            explicit FieldIterator(std::string_view payload, std::size_t start, bool atEnd);

            void computeCurrent() noexcept;

            std::string_view m_payload{};
            std::size_t m_start = 0;
            std::size_t m_next = 0;
            bool m_atEnd = true;
            std::string_view m_current{};
        };

        explicit Tokenizer(std::string_view sentence);

        // True when structurally valid AND checksum matches.
        bool valid() const noexcept;

        // Structural status. Note: checksum mismatch does not set an error code; use checksumValid().
        NmeaParseStatus status() const noexcept;

        std::string_view sentence() const noexcept;

        std::string_view identifier() const noexcept;   // e.g. "GPGGA"
        bool checksumValid() const noexcept;

        // Count of data fields AFTER the identifier (does not include the identifier).
        std::size_t fieldCount() const noexcept;

        FieldIterator begin() const noexcept;
        FieldIterator end() const noexcept;

    private:
        void parse();

        static bool isHexDigit(char c) noexcept;
        static int hexValue(char c) noexcept;
        static std::uint8_t computeXor(std::string_view betweenDollarAndStar) noexcept;

        std::string_view m_sentence{};
        std::string_view m_identifier{};
        std::string_view m_payload{};
        bool m_hasPayload = false;

        std::size_t m_fieldCount = 0;

        bool m_checksumValid = false;
        NmeaParseStatus m_status{};
    };
} // namespace nmea
