// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "NMEAInsertionStream.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace weather
{
    static bool isFinite(double v) noexcept
    {
        return std::isfinite(v);
    }

    InsertionStream::InsertionStream(std::string_view identifier)
    {
        if (identifier.size() != 5)
        {
            setError(NmeaParseErrorCode::InvalidIdentifierLength);
            return;
        }

        m_sentence.reserve(64);
        m_sentence.push_back('$');
        m_sentence.append(identifier.data(), identifier.size());
    }

    weather::NmeaParseStatus InsertionStream::status() const noexcept
    {
        return m_status;
    }

    void InsertionStream::setError(NmeaParseErrorCode code) noexcept
    {
        if (m_status.ok())
        {
            m_status.code = code;
        }
    }

    void InsertionStream::appendFieldPrefix()
    {
        m_sentence.push_back(',');
    }

    NmeaParseStatus InsertionStream::writeString(std::string_view v)
    {
        if (!m_status.ok()) return m_status;
        if (m_finalized) { setError(NmeaParseErrorCode::AlreadyFinalized); return m_status; }

        appendFieldPrefix();
        m_sentence.append(v.data(), v.size());
        return m_status;
    }

    NmeaParseStatus InsertionStream::writeChar(char v)
    {
        if (!m_status.ok()) return m_status;
        if (m_finalized) { setError(NmeaParseErrorCode::AlreadyFinalized); return m_status; }

        appendFieldPrefix();
        m_sentence.push_back(v);
        return m_status;
    }

    NmeaParseStatus InsertionStream::writeInt(int v)
    {
        if (!m_status.ok()) return m_status;
        if (m_finalized) { setError(NmeaParseErrorCode::AlreadyFinalized); return m_status; }

        appendFieldPrefix();

        std::array<char, 24> buf{};
        auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
        if (res.ec != std::errc())
        {
            setError(NmeaParseErrorCode::InvalidInteger);
            return m_status;
        }

        m_sentence.append(buf.data(), static_cast<std::size_t>(res.ptr - buf.data()));
        return m_status;
    }

    NmeaParseStatus InsertionStream::writeDouble(double v)
    {
        if (!m_status.ok()) return m_status;
        if (m_finalized) { setError(NmeaParseErrorCode::AlreadyFinalized); return m_status; }

        if (!isFinite(v))
        {
            setError(NmeaParseErrorCode::InvalidDouble);
            return m_status;
        }

        appendFieldPrefix();

#if defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L)
        {
            std::array<char, 64> buf{};
            auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v, std::chars_format::general);
            if (res.ec == std::errc())
            {
                m_sentence.append(buf.data(), static_cast<std::size_t>(res.ptr - buf.data()));
                return m_status;
            }
        }
#endif
        // Fallback: keep it simple and iostream-free.
        std::array<char, 64> buf{};
        int n = std::snprintf(buf.data(), buf.size(), "%.6f", v);
        if (n <= 0)
        {
            setError(NmeaParseErrorCode::InvalidDouble);
            return m_status;
        }

        std::size_t len = static_cast<std::size_t>(n);
        while (len > 0 && buf[len - 1] == '0') --len;
        if (len > 0 && buf[len - 1] == '.') --len;

        m_sentence.append(buf.data(), len);
        return m_status;
    }

    NmeaParseStatus InsertionStream::writeOptionalString(const std::optional<std::string_view>& v)
    {
        if (!v.has_value()) return writeEmpty();
        return writeString(*v);
    }

    NmeaParseStatus InsertionStream::writeOptionalChar(const std::optional<char>& v)
    {
        if (!v.has_value()) return writeEmpty();
        return writeChar(*v);
    }

    NmeaParseStatus InsertionStream::writeOptionalInt(const std::optional<int>& v)
    {
        if (!v.has_value()) return writeEmpty();
        return writeInt(*v);
    }

    NmeaParseStatus InsertionStream::writeOptionalDouble(const std::optional<double>& v)
    {
        if (!v.has_value()) return writeEmpty();
        return writeDouble(*v);
    }

    NmeaParseStatus InsertionStream::writeEmpty()
    {
        if (!m_status.ok()) return m_status;
        if (m_finalized) { setError(NmeaParseErrorCode::AlreadyFinalized); return m_status; }

        appendFieldPrefix();
        return m_status;
    }

    std::uint8_t InsertionStream::computeXor(std::string_view s) noexcept
    {
        std::uint8_t v = 0;
        for (char c : s)
        {
            v ^= static_cast<std::uint8_t>(c);
        }
        return v;
    }

    void InsertionStream::appendChecksum(std::string& out, std::uint8_t checksum)
    {
        static const char* hex = "0123456789ABCDEF";
        out.push_back('*');
        out.push_back(hex[(checksum >> 4) & 0xF]);
        out.push_back(hex[checksum & 0xF]);
    }

    NmeaParseStatus InsertionStream::finalize(bool appendCRLF)
    {
        if (!m_status.ok()) return m_status;
        if (m_finalized) return m_status;

        if (m_sentence.empty() || m_sentence.front() != '$')
        {
            setError(NmeaParseErrorCode::MissingStartDelimiter);
            return m_status;
        }

        const std::string_view between(m_sentence.data() + 1, m_sentence.size() - 1);
        const std::uint8_t cs = computeXor(between);
        appendChecksum(m_sentence, cs);

        if (appendCRLF)
        {
            m_sentence.append("\r\n");
        }

        m_finalized = true;
        return m_status;
    }

    bool InsertionStream::finalized() const noexcept
    {
        return m_finalized;
    }

    const std::string& InsertionStream::sentence() const noexcept
    {
        return m_sentence;
    }
} // namespace nmea
