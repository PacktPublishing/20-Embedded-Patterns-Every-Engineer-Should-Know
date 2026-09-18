// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"

namespace ch17
{

template<std::size_t Capacity>
struct BoundedAsciiString
{
    static_assert(Capacity <= 255);
    static constexpr std::size_t maxSize = Capacity;

    std::uint8_t size{};
    std::array<char, Capacity> value{};

    [[nodiscard]] std::string_view view() const noexcept
    {
        return {value.data(), size};
    }

    [[nodiscard]] static std::optional<BoundedAsciiString>
    make(std::string_view text) noexcept
    {
        if (text.size() > Capacity)
        {
            return std::nullopt;
        }

        BoundedAsciiString result;
        result.size = static_cast<std::uint8_t>(text.size());

        for (std::size_t index = 0; index < text.size(); ++index)
        {
            if (static_cast<unsigned char>(text[index]) > 0x7fu)
            {
                return std::nullopt;
            }
            result.value[index] = text[index];
        }
        return result;
    }

    constexpr bool operator==(const BoundedAsciiString&) const = default;
};

using ShortString = BoundedAsciiString<16>;
using MediumString = BoundedAsciiString<32>;
using LongString = BoundedAsciiString<255>;

template<std::size_t Capacity>
[[nodiscard]] bool writeBds(
    pbook::BinaryWriteStream& stream,
    const BoundedAsciiString<Capacity>& text) noexcept
{
    stream.writeUInt8(text.size)
          .writeBytes(pbook::asBytes(text.value));
    return stream.ok();
}

template<std::size_t Capacity>
[[nodiscard]] bool readBds(
    pbook::BinaryReadStream& stream,
    BoundedAsciiString<Capacity>& text) noexcept
{
    BoundedAsciiString<Capacity> decoded;
    pbook::ImmutableByteView characters;
    stream.readUInt8(decoded.size)
          .readBytesView(static_cast<std::uint32_t>(Capacity), characters);
    if (!stream.ok() || decoded.size > Capacity)
    {
        return false;
    }

    for (std::size_t index = 0; index < Capacity; ++index)
    {
        const auto character = std::to_integer<unsigned char>(characters[index]);
        if ((index < decoded.size && character > 0x7fu) ||
            (index >= decoded.size && character != 0u))
        {
            return false;
        }
        decoded.value[index] = static_cast<char>(character);
    }

    text = decoded;
    return true;
}

static_assert(ShortString::maxSize == 16);
static_assert(MediumString::maxSize == 32);
static_assert(LongString::maxSize == 255);

} // namespace ch17
