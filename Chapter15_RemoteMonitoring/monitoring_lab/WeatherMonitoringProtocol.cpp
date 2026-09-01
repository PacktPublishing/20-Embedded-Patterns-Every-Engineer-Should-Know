// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "WeatherMonitoringProtocol.h"

#include <charconv>
#include <cstdio>

namespace ch15
{
namespace
{

int hexValue(char c) noexcept
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    return -1;
}

} // namespace

std::uint8_t nmeaChecksum(std::string_view body) noexcept
{
    std::uint8_t checksum{};
    for (const unsigned char c : body)
    {
        checksum ^= c;
    }
    return checksum;
}

bool buildSentence(
    std::string_view body,
    SentenceBuffer& destination,
    std::size_t& length) noexcept
{
    length = 0;

    const auto checksum = nmeaChecksum(body);
    const int written = std::snprintf(
        destination.data(),
        destination.size(),
        "$%.*s*%02X\r\n",
        static_cast<int>(body.size()),
        body.data(),
        static_cast<unsigned int>(checksum));

    if (written < 0 || static_cast<std::size_t>(written) >= destination.size())
    {
        destination[0] = '\0';
        return false;
    }

    length = static_cast<std::size_t>(written);
    return true;
}

ParseStatus parseSentence(
    std::string_view sentence,
    ParsedSentence& parsed) noexcept
{
    parsed = {};

    while (!sentence.empty() &&
           (sentence.back() == '\r' || sentence.back() == '\n'))
    {
        sentence.remove_suffix(1);
    }

    if (sentence.empty())
    {
        return ParseStatus::Empty;
    }
    if (sentence.front() != '$')
    {
        return ParseStatus::MissingStart;
    }

    const auto star = sentence.rfind('*');
    if (star == std::string_view::npos || star + 3 != sentence.size())
    {
        return ParseStatus::MissingChecksum;
    }

    const int high = hexValue(sentence[star + 1]);
    const int low = hexValue(sentence[star + 2]);
    if (high < 0 || low < 0)
    {
        return ParseStatus::BadChecksumText;
    }

    const auto body = sentence.substr(1, star - 1);
    const auto expected = static_cast<std::uint8_t>((high << 4) | low);
    if (nmeaChecksum(body) != expected)
    {
        return ParseStatus::ChecksumMismatch;
    }

    const auto firstComma = body.find(',');
    parsed.identifier = body.substr(0, firstComma);
    if (parsed.identifier.size() != 5)
    {
        return ParseStatus::BadIdentifier;
    }

    if (firstComma == std::string_view::npos)
    {
        return ParseStatus::Ok;
    }

    std::size_t start = firstComma + 1;
    while (start <= body.size())
    {
        if (parsed.fieldCount >= parsed.fields.size())
        {
            parsed = {};
            return ParseStatus::TooManyFields;
        }

        const auto comma = body.find(',', start);
        if (comma == std::string_view::npos)
        {
            parsed.fields[parsed.fieldCount++] = body.substr(start);
            break;
        }

        parsed.fields[parsed.fieldCount++] = body.substr(start, comma - start);
        start = comma + 1;

        if (start == body.size())
        {
            if (parsed.fieldCount >= parsed.fields.size())
            {
                parsed = {};
                return ParseStatus::TooManyFields;
            }
            parsed.fields[parsed.fieldCount++] = {};
            break;
        }
    }

    return ParseStatus::Ok;
}

std::string_view parseStatusName(ParseStatus status) noexcept
{
    switch (status)
    {
    case ParseStatus::Ok: return "OK";
    case ParseStatus::Empty: return "EMPTY";
    case ParseStatus::MissingStart: return "MISSING_START";
    case ParseStatus::MissingChecksum: return "MISSING_CHECKSUM";
    case ParseStatus::BadChecksumText: return "BAD_CHECKSUM_TEXT";
    case ParseStatus::ChecksumMismatch: return "CHECKSUM_MISMATCH";
    case ParseStatus::BadIdentifier: return "BAD_IDENTIFIER";
    case ParseStatus::TooManyFields: return "TOO_MANY_FIELDS";
    }
    return "UNKNOWN";
}

std::optional<ReportType> parseReportType(std::string_view text) noexcept
{
    if (text == "STS") return ReportType::Status;
    if (text == "HLT") return ReportType::Health;
    if (text == "TMP") return ReportType::Temperature;
    if (text == "PRS") return ReportType::Pressure;
    if (text == "WND") return ReportType::Wind;
    if (text == "DIA") return ReportType::Diagnostic;
    return std::nullopt;
}

std::string_view reportTypeCode(ReportType type) noexcept
{
    switch (type)
    {
    case ReportType::Status: return "STS";
    case ReportType::Health: return "HLT";
    case ReportType::Temperature: return "TMP";
    case ReportType::Pressure: return "PRS";
    case ReportType::Wind: return "WND";
    case ReportType::Diagnostic: return "DIA";
    }
    return "???";
}

std::string_view reportIdentifier(ReportType type) noexcept
{
    switch (type)
    {
    case ReportType::Status: return "WXSTS";
    case ReportType::Health: return "WXHLT";
    case ReportType::Temperature: return "WXTMP";
    case ReportType::Pressure: return "WXPRS";
    case ReportType::Wind: return "WXWND";
    case ReportType::Diagnostic: return "WXDIA";
    }
    return "WX???";
}

bool parseUnsigned(std::string_view text, std::uint32_t& value) noexcept
{
    value = 0;
    if (text.empty())
    {
        return false;
    }

    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

} // namespace ch15
