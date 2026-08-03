// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "BdsCommon.h"

#include <cstring>

//------------------------------------------------------------------------------
// Helpers for working with byte-view buffers
//------------------------------------------------------------------------------

std::uint8_t* u8(pbook::MutableByteView bytes) noexcept
{
    return reinterpret_cast<std::uint8_t*>(bytes.data());
}

const std::uint8_t* u8(pbook::ImmutableByteView bytes) noexcept
{
    return reinterpret_cast<const std::uint8_t*>(bytes.data());
}

pbook::MutableByteView subview(
    pbook::MutableByteView bytes,
    std::size_t offset,
    std::size_t length) noexcept
{
    if (offset > bytes.size())
    {
        return {};
    }

    const std::size_t available = bytes.size() - offset;
    const std::size_t viewSize =
        (length <= available) ? length : available;

    return pbook::MutableByteView(
        bytes.data() + offset,
        viewSize);
}

pbook::ImmutableByteView subview(
    pbook::ImmutableByteView bytes,
    std::size_t offset,
    std::size_t length) noexcept
{
    if (offset > bytes.size())
    {
        return {};
    }

    const std::size_t available = bytes.size() - offset;
    const std::size_t viewSize =
        (length <= available) ? length : available;

    return pbook::ImmutableByteView(
        bytes.data() + offset,
        viewSize);
}

//------------------------------------------------------------------------------
// Endianness
//------------------------------------------------------------------------------

Endianness detectHostEndianness() noexcept
{
    const std::uint32_t value = 0x01020304u;
    const auto* firstByte =
        reinterpret_cast<const std::uint8_t*>(&value);

    return (firstByte[0] == 0x04u)
               ? Endianness::Little
               : Endianness::Big;
}

//------------------------------------------------------------------------------
// Safe copying with optional byte reversal
//------------------------------------------------------------------------------

void copyForward(
    std::uint8_t* destination,
    const std::uint8_t* source,
    std::size_t size) noexcept
{
    std::memcpy(destination, source, size);
}

void copyReversed(
    std::uint8_t* destination,
    const std::uint8_t* source,
    std::size_t size) noexcept
{
    for (std::size_t index = 0; index < size; ++index)
    {
        destination[index] = source[size - 1u - index];
    }
}

//------------------------------------------------------------------------------
// CRC-16/CCITT-FALSE
//------------------------------------------------------------------------------

std::uint16_t crc16CcittFalse(
    const std::uint8_t* data,
    std::size_t size) noexcept
{
    std::uint16_t crc = Crc16CcittInitialValue;

    for (std::size_t index = 0; index < size; ++index)
    {
        crc ^= static_cast<std::uint16_t>(data[index]) << 8u;

        for (unsigned bit = 0; bit < 8u; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
            {
                crc = static_cast<std::uint16_t>(
                    (crc << 1u) ^ Crc16CcittPolynomial);
            }
            else
            {
                crc = static_cast<std::uint16_t>(crc << 1u);
            }
        }
    }

    return crc;
}

std::uint16_t crc16CcittFalse(
    pbook::ImmutableByteView bytes) noexcept
{
    return crc16CcittFalse(
        u8(bytes),
        bytes.size());
}
