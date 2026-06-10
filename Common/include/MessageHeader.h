// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Wilson

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "BdsCommon.h"
#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"

//
//------------------------------------------------------------------------------
// Fixed-size framing header with checksums
//
// Header wire endianness is fixed by spec; payload endianness declared in header.
// Reserved fields/bits must be zero in v1 and ignored by receivers.
//------------------------------------------------------------------------------


static constexpr Endianness HeaderWireEndianness = Endianness::Little;

static constexpr std::uint32_t MessageMagicV1 = 0x31534442; // 'B''D''S''1' in LE

struct MessageHeaderV1
{
    std::uint32_t magic = MessageMagicV1;
    std::uint8_t  version = 1;
    std::uint8_t  headerSize = 0;       // = sizeof(MessageHeaderV1)
    std::uint8_t  payloadEndian = 0;    // 0=Little, 1=Big
    std::uint8_t  headerFlags = 0;      // reserved bits (must be 0 in v1)

    std::uint16_t serviceId = 0;
    std::uint16_t messageType = 0;

    std::uint32_t payloadSize = 0;

    std::uint32_t flags = 0;            // reserved in v1

    std::uint8_t  headerChecksum = 0;   // XOR of header bytes with this treated as 0
    std::uint8_t  payloadChecksum = 0;  // XOR of payload bytes
    std::uint16_t reserved = 0;         // must be 0 in v1
};

static_assert(sizeof(MessageHeaderV1) == 24, "MessageHeaderV1 must be fixed size");
static_assert(std::is_standard_layout<MessageHeaderV1>::value,
              "MessageHeaderV1 must be standard-layout for offsetof()");

inline Endianness payloadEndianFromHeader(uint8_t payloadEndian) noexcept
{
    return (payloadEndian == 0) ? Endianness::Little : Endianness::Big;
}

inline uint8_t computeHeaderChecksum(const MessageHeaderV1& h) noexcept
{
    uint8_t tmp[sizeof(MessageHeaderV1)];
    std::memcpy(tmp, &h, sizeof(tmp));

    tmp[offsetof(MessageHeaderV1, headerChecksum)] = 0;

    return xorChecksum(tmp, sizeof(tmp));
}

inline void finalizeChecksums(MessageHeaderV1& h, ImmutableByteView payload) noexcept
{
    h.headerSize = static_cast<uint8_t>(sizeof(MessageHeaderV1));
    h.payloadChecksum = xorChecksum(payload);
    h.headerChecksum = computeHeaderChecksum(h);
}

inline weather::BinaryWriteStream& writeHeaderV1(weather::BinaryWriteStream& w, const MessageHeaderV1& h) noexcept
{
    w.writeUInt32(h.magic)
        .writeUInt8(h.version)
        .writeUInt8(h.headerSize)
        .writeUInt8(h.payloadEndian)
        .writeUInt8(h.headerFlags)
        .writeUInt16(h.serviceId)
        .writeUInt16(h.messageType)
        .writeUInt32(h.payloadSize)
        .writeUInt32(h.flags)
        .writeUInt8(h.headerChecksum)
        .writeUInt8(h.payloadChecksum)
        .writeUInt16(h.reserved);

    return w;
}

inline weather::BinaryReadStream& readHeaderV1(weather::BinaryReadStream& r, MessageHeaderV1& h) noexcept
{
    r.readUInt32(h.magic)
        .readUInt8(h.version)
        .readUInt8(h.headerSize)
        .readUInt8(h.payloadEndian)
        .readUInt8(h.headerFlags)
        .readUInt16(h.serviceId)
        .readUInt16(h.messageType)
        .readUInt32(h.payloadSize)
        .readUInt32(h.flags)
        .readUInt8(h.headerChecksum)
        .readUInt8(h.payloadChecksum)
        .readUInt16(h.reserved);

    return r;
}

inline bool validateHeaderV1(const MessageHeaderV1& h) noexcept
{
    if ( h.magic != MessageMagicV1 )
    {
        return false;
    }
    if (h.version != 1)
    {
        return false;
    }
    if (h.headerSize != static_cast<std::uint8_t>(sizeof(MessageHeaderV1)))
    {
        return false;
    }
    if (h.headerFlags != 0 || h.flags != 0 || h.reserved != 0)
    {
        return false;
    }

    return computeHeaderChecksum(h) == h.headerChecksum;
}

inline bool validatePayloadChecksum(const MessageHeaderV1& h, ImmutableByteView payload) noexcept
{
    (void)h;
    return xorChecksum(payload) == h.payloadChecksum;
}
