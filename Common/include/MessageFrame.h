// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
#include "ImmutableByteView.h"
#include "MessageHeader.h"
#include "MutableByteView.h"

//------------------------------------------------------------------------------
// Complete Version 1 BDS frame helpers
//
// These helpers operate on one complete frame already present in memory. They
// do not perform device or socket I/O and they are not incremental stream
// framers. They are especially useful with message-oriented transports such as
// UDP, where one datagram can contain one complete BDS frame.
//------------------------------------------------------------------------------

enum class MessageFrameStatus : std::uint8_t
{
    Ok = 0,
    OutputTooSmall,
    PayloadTooLarge,
    FrameTooSmall,
    InvalidHeader,
    InvalidFrameSize,
    InvalidPayloadCrc
};

inline MessageFrameStatus writeFrameV1(
    pbook::MutableByteView destination,
    MessageHeaderV1 header,
    pbook::ImmutableByteView payload,
    std::size_t& bytesWritten) noexcept
{
    bytesWritten = 0u;

    if (payload.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return MessageFrameStatus::PayloadTooLarge;
    }

    if (payload.size() >
        std::numeric_limits<std::size_t>::max() - MessageHeaderV1WireSize)
    {
        return MessageFrameStatus::PayloadTooLarge;
    }

    const std::size_t required = MessageHeaderV1WireSize + payload.size();
    if (destination.size() < required)
    {
        return MessageFrameStatus::OutputTooSmall;
    }

    finalizeCrcs(header, payload);

    pbook::BinaryWriteStream writer(
        destination,
        HeaderWireEndianness);

    writeHeaderV1(writer, header);
    writer.writeBytes(payload);

    if (!writer.ok() || writer.bytesWritten() != required)
    {
        return MessageFrameStatus::OutputTooSmall;
    }

    bytesWritten = writer.bytesWritten();
    return MessageFrameStatus::Ok;
}

inline MessageFrameStatus readFrameV1(
    pbook::ImmutableByteView frame,
    MessageHeaderV1& header,
    pbook::ImmutableByteView& payload) noexcept
{
    payload = {};

    if (frame.size() < MessageHeaderV1WireSize)
    {
        return MessageFrameStatus::FrameTooSmall;
    }

    const pbook::ImmutableByteView headerBytes{
        frame.data(),
        MessageHeaderV1WireSize};

    pbook::BinaryReadStream reader(
        headerBytes,
        HeaderWireEndianness);

    readHeaderV1(reader, header);

    if (!reader.ok() ||
        reader.bytesRead() != MessageHeaderV1WireSize ||
        !validateHeaderV1(header))
    {
        return MessageFrameStatus::InvalidHeader;
    }

    const std::size_t expectedSize =
        MessageHeaderV1WireSize +
        static_cast<std::size_t>(header.payloadSize);

    if (frame.size() != expectedSize)
    {
        return MessageFrameStatus::InvalidFrameSize;
    }

    payload = pbook::ImmutableByteView{
        frame.data() + MessageHeaderV1WireSize,
        static_cast<std::size_t>(header.payloadSize)};

    if (!validatePayloadCrc(header, payload))
    {
        payload = {};
        return MessageFrameStatus::InvalidPayloadCrc;
    }

    return MessageFrameStatus::Ok;
}
