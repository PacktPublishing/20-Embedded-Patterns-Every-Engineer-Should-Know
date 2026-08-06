// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "BdsCommon.h"
#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"

//------------------------------------------------------------------------------
// Fixed-size framing header with CRC-16 integrity checks
//
// Header wire endianness is fixed by the protocol. Payload endianness is
// declared in the header.
//
// The wire format is encoded and decoded field by field. The in-memory C++
// structure is not copied directly to or from the wire and its sizeof() value
// is not part of the protocol definition.
//
// CRC-16 detects accidental corruption. It does not provide authentication or
// protection against deliberate modification.
//------------------------------------------------------------------------------

inline constexpr Endianness HeaderWireEndianness = Endianness::Little;

inline constexpr std::uint32_t MessageMagicV1 =
    0x31534442u; // 'B''D''S''1' in little-endian wire order

inline constexpr std::uint8_t MessageVersionV1 = 1u;
inline constexpr std::size_t MessageHeaderV1WireSize = 24u;

struct MessageHeaderV1
{
    std::uint32_t magic{MessageMagicV1};
    std::uint8_t version{MessageVersionV1};
    std::uint8_t headerSize{
                            static_cast<std::uint8_t>(MessageHeaderV1WireSize)};
    std::uint8_t payloadEndian{0}; // 0=Little, 1=Big
    std::uint8_t headerFlags{0};   // Reserved; must be zero in v1

    std::uint16_t serviceId{0};
    std::uint16_t messageType{0};

    std::uint32_t payloadSize{0};

    std::uint32_t flags{0}; // Reserved; must be zero in v1

    std::uint16_t headerCrc{0};
    std::uint16_t payloadCrc{0};
};

//------------------------------------------------------------------------------
// Payload endianness
//------------------------------------------------------------------------------

inline bool isValidPayloadEndian(std::uint8_t value) noexcept
{
    return value == 0u || value == 1u;
}

inline Endianness payloadEndianFromHeader(
    std::uint8_t payloadEndian) noexcept
{
    return payloadEndian == 0u
               ? Endianness::Little
               : Endianness::Big;
}

//------------------------------------------------------------------------------
// Header encoding and decoding
//------------------------------------------------------------------------------

inline pbook::BinaryWriteStream& writeHeaderV1(
    pbook::BinaryWriteStream& writer,
    const MessageHeaderV1& header) noexcept
{
    writer.writeUInt32(header.magic)
    .writeUInt8(header.version)
        .writeUInt8(header.headerSize)
        .writeUInt8(header.payloadEndian)
        .writeUInt8(header.headerFlags)
        .writeUInt16(header.serviceId)
        .writeUInt16(header.messageType)
        .writeUInt32(header.payloadSize)
        .writeUInt32(header.flags)
        .writeUInt16(header.headerCrc)
        .writeUInt16(header.payloadCrc);

    return writer;
}

inline pbook::BinaryReadStream& readHeaderV1(
    pbook::BinaryReadStream& reader,
    MessageHeaderV1& header) noexcept
{
    reader.readUInt32(header.magic)
    .readUInt8(header.version)
        .readUInt8(header.headerSize)
        .readUInt8(header.payloadEndian)
        .readUInt8(header.headerFlags)
        .readUInt16(header.serviceId)
        .readUInt16(header.messageType)
        .readUInt32(header.payloadSize)
        .readUInt32(header.flags)
        .readUInt16(header.headerCrc)
        .readUInt16(header.payloadCrc);

    return reader;
}

//------------------------------------------------------------------------------
// Header CRC
//------------------------------------------------------------------------------
//
// The header CRC is calculated over the serialized 24-byte wire header with
// headerCrc set to zero. The payload CRC remains present in those bytes, so the
// payload CRC must be calculated before the header CRC.
//------------------------------------------------------------------------------

inline std::uint16_t computeHeaderCrc(
    const MessageHeaderV1& header) noexcept
{
    std::array<std::byte, MessageHeaderV1WireSize> wireBytes{};

    MessageHeaderV1 headerForCrc = header;
    headerForCrc.headerCrc = 0u;

    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(
            wireBytes.data(),
            wireBytes.size()),
        HeaderWireEndianness);

    writeHeaderV1(writer, headerForCrc);

    if (!writer.ok() ||
        writer.bytesWritten() != MessageHeaderV1WireSize)
    {
        return 0u;
    }

    return crc16CcittFalse(
        pbook::ImmutableByteView(
            wireBytes.data(),
            wireBytes.size()));
}

//------------------------------------------------------------------------------
// CRC finalization
//------------------------------------------------------------------------------

inline void finalizeCrcs(
    MessageHeaderV1& header,
    pbook::ImmutableByteView payload) noexcept
{
    header.headerSize =
        static_cast<std::uint8_t>(MessageHeaderV1WireSize);

    header.payloadSize =
        static_cast<std::uint32_t>(payload.size());

    header.payloadCrc = crc16CcittFalse(payload);

    // Calculate this last because the header CRC covers payloadCrc.
    header.headerCrc = computeHeaderCrc(header);
}

//------------------------------------------------------------------------------
// Validation
//------------------------------------------------------------------------------

inline bool validateHeaderV1(
    const MessageHeaderV1& header) noexcept
{
    if (header.magic != MessageMagicV1)
    {
        return false;
    }

    if (header.version != MessageVersionV1)
    {
        return false;
    }

    if (header.headerSize !=
        static_cast<std::uint8_t>(MessageHeaderV1WireSize))
    {
        return false;
    }

    if (!isValidPayloadEndian(header.payloadEndian))
    {
        return false;
    }

    if (header.headerFlags != 0u || header.flags != 0u)
    {
        return false;
    }

    return computeHeaderCrc(header) == header.headerCrc;
}

inline bool validatePayloadCrc(
    const MessageHeaderV1& header,
    pbook::ImmutableByteView payload) noexcept
{
    if (payload.size() != header.payloadSize)
    {
        return false;
    }

    return crc16CcittFalse(payload) == header.payloadCrc;
}
