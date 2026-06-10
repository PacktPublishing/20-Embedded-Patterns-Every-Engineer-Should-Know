// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "NMEATokenizer.h"
#include "Status.h"

#include <algorithm>

namespace weather
{
    Tokenizer::FieldIterator::FieldIterator(std::string_view payload, std::size_t start, bool atEnd)
        : m_payload(payload)
        , m_start(start)
        , m_atEnd(atEnd)
    {
        if (!m_atEnd)
        {
            computeCurrent();
        }
    }

    void Tokenizer::FieldIterator::computeCurrent() noexcept
    {
        if (m_start > m_payload.size())
        {
            m_atEnd = true;
            m_current = std::string_view{};
            return;
        }

        const std::size_t comma = m_payload.find(',', m_start);
        if (comma == std::string_view::npos)
        {
            m_current = m_payload.substr(m_start);
            m_next = m_payload.size() + 1; // sentinel
            return;
        }

        m_current = m_payload.substr(m_start, comma - m_start);
        m_next = comma + 1;
    }

    std::string_view Tokenizer::FieldIterator::operator*() const noexcept
    {
        return m_current;
    }

    Tokenizer::FieldIterator& Tokenizer::FieldIterator::operator++() noexcept
    {
        if (m_atEnd)
        {
            return *this;
        }

        if (m_next == m_payload.size() + 1)
        {
            m_atEnd = true;
            m_current = std::string_view{};
            return *this;
        }

        m_start = m_next;

        if (m_start > m_payload.size())
        {
            m_atEnd = true;
            m_current = std::string_view{};
            return *this;
        }

        computeCurrent();
        return *this;
    }

    bool Tokenizer::FieldIterator::operator==(const FieldIterator& rhs) const noexcept
    {
        if (m_atEnd && rhs.m_atEnd) return true;

        return m_atEnd == rhs.m_atEnd &&
               m_payload.data() == rhs.m_payload.data() &&
               m_payload.size() == rhs.m_payload.size() &&
               m_start == rhs.m_start;
    }

    bool Tokenizer::FieldIterator::operator!=(const FieldIterator& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    Tokenizer::Tokenizer(std::string_view sentence)
        : m_sentence(sentence)
    {
        parse();
    }

    bool Tokenizer::valid() const noexcept
    {
        return m_status.ok() && m_checksumValid;
    }

    NmeaParseStatus Tokenizer::status() const noexcept
    {
        return m_status;
    }

    std::string_view Tokenizer::sentence() const noexcept
    {
        return m_sentence;
    }

    std::string_view Tokenizer::identifier() const noexcept
    {
        return m_identifier;
    }

    bool Tokenizer::checksumValid() const noexcept
    {
        return m_checksumValid;
    }

    std::size_t Tokenizer::fieldCount() const noexcept
    {
        return m_fieldCount;
    }

    Tokenizer::FieldIterator Tokenizer::begin() const noexcept
    {
        if (!m_hasPayload)
        {
            return end();
        }
        return FieldIterator(m_payload, 0, false);
    }

    Tokenizer::FieldIterator Tokenizer::end() const noexcept
    {
        return FieldIterator(m_payload, 0, true);
    }

    bool Tokenizer::isHexDigit(char c) noexcept
    {
        return (c >= '0' && c <= '9') ||
               (c >= 'A' && c <= 'F') ||
               (c >= 'a' && c <= 'f');
    }

    int Tokenizer::hexValue(char c) noexcept
    {
        if (c >= '0' && c <= '9') return (c - '0');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        return -1;
    }

    std::uint8_t Tokenizer::computeXor(std::string_view s) noexcept
    {
        std::uint8_t v = 0;
        for (char c : s)
        {
            v ^= static_cast<std::uint8_t>(c);
        }
        return v;
    }

    void Tokenizer::parse()
    {
        m_identifier = std::string_view{};
        m_payload = std::string_view{};
        m_hasPayload = false;
        m_fieldCount = 0;
        m_checksumValid = false;
        m_status = NmeaParseStatus{};

        if (m_sentence.empty())
        {
            m_status.code = NmeaParseErrorCode::EmptyInput;
            return;
        }

        while (!m_sentence.empty() && (m_sentence.back() == '\n' || m_sentence.back() == '\r'))
        {
            m_sentence.remove_suffix(1);
        }
        if (m_sentence.empty())
        {
            m_status.code = NmeaParseErrorCode::EmptyInput;
            return;
        }

        if (m_sentence.front() != '$')
        {
            m_status.code = NmeaParseErrorCode::MissingStartDelimiter;
            m_status.context = m_sentence;
            return;
        }

        const std::size_t starPos = m_sentence.find('*');
        if (starPos == std::string_view::npos)
        {
            m_status.code = NmeaParseErrorCode::MissingChecksumDelimiter;
            m_status.context = m_sentence;
            return;
        }

        if (starPos + 2 >= m_sentence.size())
        {
            m_status.code = NmeaParseErrorCode::MissingChecksumValue;
            m_status.context = m_sentence.substr(starPos);
            return;
        }

        const char h1 = m_sentence[starPos + 1];
        const char h2 = m_sentence[starPos + 2];
        if (!isHexDigit(h1) || !isHexDigit(h2))
        {
            m_status.code = NmeaParseErrorCode::InvalidChecksumValue;
            m_status.context = m_sentence.substr(starPos, std::min<std::size_t>(3, m_sentence.size() - starPos));
            return;
        }

        const int hi = hexValue(h1);
        const int lo = hexValue(h2);
        const std::uint8_t expected = static_cast<std::uint8_t>((hi << 4) | lo);

        const std::string_view between = m_sentence.substr(1, starPos - 1);
        const std::uint8_t actual = computeXor(between);
        m_checksumValid = (actual == expected);

        if (between.size() < 5)
        {
            m_status.code = NmeaParseErrorCode::MissingIdentifier;
            m_status.context = between;
            return;
        }

        m_identifier = between.substr(0, 5);

        if (between.size() == 5)
        {
            m_hasPayload = false;
            m_payload = std::string_view{};
            m_fieldCount = 0;
            return;
        }

        if (between[5] != ',')
        {
            m_status.code = NmeaParseErrorCode::InvalidIdentifierLength;
            m_status.context = between.substr(0, std::min<std::size_t>(16, between.size()));
            return;
        }

        m_hasPayload = true;
        m_payload = between.substr(6); // may be empty

        std::size_t commas = 0;
        for (char c : m_payload)
        {
            if (c == ',') ++commas;
        }
        m_fieldCount = commas + 1;
    }
} // namespace nmea
