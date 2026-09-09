// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"

namespace ch16
{

enum class ParameterID : std::uint16_t
{
    ReportingInterval,
    TemperatureLimit,
    OperatingMode,
    Count
};

inline constexpr std::size_t parameterCount =
    static_cast<std::size_t>(ParameterID::Count);

constexpr std::size_t toIndex(ParameterID id) noexcept
{
    return static_cast<std::size_t>(id);
}

constexpr bool isValidParameterId(std::uint16_t raw) noexcept
{
    return raw < static_cast<std::uint16_t>(ParameterID::Count);
}

enum class ParameterSource : std::uint8_t
{
    CompiledDefault,
    PersistentStorage,
    ConfigurationFile,
    CommandLine,
    Runtime
};

constexpr std::uint8_t precedence(ParameterSource source) noexcept
{
    switch (source)
    {
    case ParameterSource::CompiledDefault:   return 0;
    case ParameterSource::PersistentStorage: return 1;
    case ParameterSource::ConfigurationFile: return 2;
    case ParameterSource::CommandLine:       return 3;
    case ParameterSource::Runtime:           return 4;
    }
    return 0;
}

enum class UpdateResult : std::uint8_t
{
    OK,
    UNKNOWN_PARAMETER,
    INVALID_VALUE,
    OUT_OF_RANGE,
    READ_ONLY,
    NOT_RUNTIME_WRITABLE,
    LOWER_PRECEDENCE
};

enum class OperatingMode : std::uint8_t
{
    Normal,
    Service,
    Safe
};

inline constexpr Endianness ParameterImageEndianness = Endianness::Little;

namespace detail
{
inline std::string_view copyText(std::string_view text, std::span<char> buffer) noexcept
{
    if (text.size() > buffer.size())
    {
        return {};
    }
    std::copy(text.begin(), text.end(), buffer.begin());
    return {buffer.data(), text.size()};
}
}

struct ReportingInterval
{
    using ValueType = std::uint32_t;

    static constexpr ParameterID id = ParameterID::ReportingInterval;
    static constexpr std::string_view name = "reporting_interval";
    static constexpr ValueType defaultValue = 1000;
    static constexpr std::size_t offset = 0;
    static constexpr std::size_t encodedSize = 4;
    static constexpr bool writable = true;
    static constexpr bool runtimeWritable = true;

    static constexpr bool validate(ValueType value) noexcept
    {
        return value >= 100 && value <= 60000;
    }

    static bool parse(std::string_view text, ValueType& value) noexcept
    {
        ValueType parsed{};
        const auto [ptr, ec] = std::from_chars(
            text.data(), text.data() + text.size(), parsed);
        if (ec != std::errc{} || ptr != text.data() + text.size())
        {
            return false;
        }
        value = parsed;
        return true;
    }

    static std::string_view serialize(ValueType value, std::span<char> buffer) noexcept
    {
        const auto [ptr, ec] = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value);
        if (ec != std::errc{})
        {
            return {};
        }
        return {buffer.data(), static_cast<std::size_t>(ptr - buffer.data())};
    }

    static void decode(pbook::BinaryReadStream& reader, ValueType& value) noexcept
    {
        reader.readUInt32(value);
    }

    static void encode(pbook::BinaryWriteStream& writer, ValueType value) noexcept
    {
        writer.writeUInt32(value);
    }
};

struct TemperatureLimit
{
    using ValueType = float;

    static constexpr ParameterID id = ParameterID::TemperatureLimit;
    static constexpr std::string_view name = "temperature_limit";
    static constexpr ValueType defaultValue = 45.0F;
    static constexpr std::size_t offset =
        ReportingInterval::offset + ReportingInterval::encodedSize;
    static constexpr std::size_t encodedSize = 4;
    static constexpr bool writable = true;
    // Demonstrates a parameter that may be configured at startup but not
    // modified through the runtime remote-control path.
    static constexpr bool runtimeWritable = false;

    static constexpr bool validate(ValueType value) noexcept
    {
        return value >= -40.0F && value <= 125.0F;
    }

    static bool parse(std::string_view text, ValueType& value) noexcept
    {
        ValueType parsed{};
        const auto [ptr, ec] = std::from_chars(
            text.data(), text.data() + text.size(), parsed,
            std::chars_format::general);
        if (ec != std::errc{} || ptr != text.data() + text.size())
        {
            return false;
        }
        value = parsed;
        return true;
    }

    static std::string_view serialize(ValueType value, std::span<char> buffer) noexcept
    {
        const auto [ptr, ec] = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value,
            std::chars_format::general);
        if (ec != std::errc{})
        {
            return {};
        }
        return {buffer.data(), static_cast<std::size_t>(ptr - buffer.data())};
    }

    static void decode(pbook::BinaryReadStream& reader, ValueType& value) noexcept
    {
        reader.readFloat(value);
    }

    static void encode(pbook::BinaryWriteStream& writer, ValueType value) noexcept
    {
        writer.writeFloat(value);
    }
};

struct OperatingModeParameter
{
    using ValueType = OperatingMode;

    static constexpr ParameterID id = ParameterID::OperatingMode;
    static constexpr std::string_view name = "operating_mode";
    static constexpr ValueType defaultValue = OperatingMode::Normal;
    static constexpr std::size_t offset =
        TemperatureLimit::offset + TemperatureLimit::encodedSize;
    static constexpr std::size_t encodedSize = 1;
    static constexpr bool writable = true;
    static constexpr bool runtimeWritable = true;

    static constexpr bool validate(ValueType value) noexcept
    {
        return value == OperatingMode::Normal ||
               value == OperatingMode::Service ||
               value == OperatingMode::Safe;
    }

    static bool parse(std::string_view text, ValueType& value) noexcept
    {
        if (text == "normal")
        {
            value = OperatingMode::Normal;
            return true;
        }
        if (text == "service")
        {
            value = OperatingMode::Service;
            return true;
        }
        if (text == "safe")
        {
            value = OperatingMode::Safe;
            return true;
        }
        return false;
    }

    static std::string_view serialize(ValueType value, std::span<char> buffer) noexcept
    {
        switch (value)
        {
        case OperatingMode::Normal:  return detail::copyText("normal", buffer);
        case OperatingMode::Service: return detail::copyText("service", buffer);
        case OperatingMode::Safe:    return detail::copyText("safe", buffer);
        }
        return {};
    }

    static void decode(pbook::BinaryReadStream& reader, ValueType& value) noexcept
    {
        std::uint8_t raw{};
        reader.readUInt8(raw);
        value = static_cast<ValueType>(raw);
    }

    static void encode(pbook::BinaryWriteStream& writer, ValueType value) noexcept
    {
        writer.writeUInt8(static_cast<std::uint8_t>(value));
    }
};

inline constexpr std::size_t ParameterImageSize =
    OperatingModeParameter::offset + OperatingModeParameter::encodedSize;

static_assert(ReportingInterval::validate(ReportingInterval::defaultValue));
static_assert(TemperatureLimit::validate(TemperatureLimit::defaultValue));
static_assert(OperatingModeParameter::validate(OperatingModeParameter::defaultValue));

constexpr std::string_view sourceName(ParameterSource source) noexcept
{
    switch (source)
    {
    case ParameterSource::CompiledDefault:   return "compiled-default";
    case ParameterSource::PersistentStorage: return "persistent-storage";
    case ParameterSource::ConfigurationFile: return "configuration-file";
    case ParameterSource::CommandLine:       return "command-line";
    case ParameterSource::Runtime:           return "runtime";
    }
    return "unknown";
}

constexpr std::string_view resultName(UpdateResult result) noexcept
{
    switch (result)
    {
    case UpdateResult::OK:                   return "OK";
    case UpdateResult::UNKNOWN_PARAMETER:    return "UNKNOWN_PARAMETER";
    case UpdateResult::INVALID_VALUE:        return "INVALID_VALUE";
    case UpdateResult::OUT_OF_RANGE:         return "OUT_OF_RANGE";
    case UpdateResult::READ_ONLY:            return "READ_ONLY";
    case UpdateResult::NOT_RUNTIME_WRITABLE: return "NOT_RUNTIME_WRITABLE";
    case UpdateResult::LOWER_PRECEDENCE:     return "LOWER_PRECEDENCE";
    }
    return "UNKNOWN_RESULT";
}

constexpr std::string_view operatingModeName(OperatingMode mode) noexcept
{
    switch (mode)
    {
    case OperatingMode::Normal:  return "normal";
    case OperatingMode::Service: return "service";
    case OperatingMode::Safe:    return "safe";
    }
    return "unknown";
}

} // namespace ch16
