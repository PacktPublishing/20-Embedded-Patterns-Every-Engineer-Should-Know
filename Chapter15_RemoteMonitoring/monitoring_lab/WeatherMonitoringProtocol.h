// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ch15
{

inline constexpr std::size_t MaxSentenceSize = 256;
inline constexpr std::size_t MaxFields = 8;

using SentenceBuffer = std::array<char, MaxSentenceSize>;

enum class ParseStatus
{
    Ok,
    Empty,
    MissingStart,
    MissingChecksum,
    BadChecksumText,
    ChecksumMismatch,
    BadIdentifier,
    TooManyFields
};

enum class ReportType
{
    Status,
    Health,
    Temperature,
    Pressure,
    Wind,
    Diagnostic
};

struct ParsedSentence
{
    std::string_view identifier;
    std::array<std::string_view, MaxFields> fields{};
    std::size_t fieldCount{};
};

std::uint8_t nmeaChecksum(std::string_view body) noexcept;

bool buildSentence(
    std::string_view body,
    SentenceBuffer& destination,
    std::size_t& length) noexcept;

ParseStatus parseSentence(
    std::string_view sentence,
    ParsedSentence& parsed) noexcept;

std::string_view parseStatusName(ParseStatus status) noexcept;

std::optional<ReportType> parseReportType(std::string_view text) noexcept;
std::string_view reportTypeCode(ReportType type) noexcept;
std::string_view reportIdentifier(ReportType type) noexcept;

bool parseUnsigned(std::string_view text, std::uint32_t& value) noexcept;

} // namespace ch15
