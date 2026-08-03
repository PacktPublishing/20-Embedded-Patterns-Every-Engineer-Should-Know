// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Wilson

//
// BinaryDataStream characterization tests
//
// These tests preserve the behavior of the original DDS implementation before
// the message header and checksum handling are revised for Chapter 11.
//

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "BdsCommon.h"
#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
#include "ImmutableByteView.h"
#include "MessageHeader.h"
#include "MutableByteView.h"

struct Sample
{
    std::uint32_t ms{};
    float temperatureC{};
    double pressurePa{};
    bool ok{};
};

static bool nearlyEqual(double lhs, double rhs, double epsilon = 1e-9)
{
    return std::abs(lhs - rhs) <= epsilon;
}

int main()
{
    // -------------------------------------------------------------------------
    // Primitive payload round trip
    // -------------------------------------------------------------------------

    std::byte storage[256]{};
    pbook::MutableByteView outputBuffer(storage, sizeof(storage));

    const Sample original{
        .ms = 123456u,
        .temperatureC = 21.25f,
        .pressurePa = 101325.125,
        .ok = true
    };

    constexpr Endianness payloadWireEndianness = Endianness::Little;
    constexpr std::string_view originalName{"TMPP"};

    weather::BinaryWriteStream writer(
        outputBuffer,
        payloadWireEndianness);

    writer.writeUInt32(original.ms)
        .writeFloat(original.temperatureC)
        .writeDouble(original.pressurePa)
        .writeBool(original.ok)
        .writeString(originalName);

    assert(writer.ok());

    const std::size_t bytesWritten = writer.bytesWritten();

    pbook::ImmutableByteView inputBuffer(storage, bytesWritten);
    weather::BinaryReadStream reader(
        inputBuffer,
        payloadWireEndianness);

    Sample decoded;
    std::string_view decodedName;

    reader.readUInt32(decoded.ms)
        .readFloat(decoded.temperatureC)
        .readDouble(decoded.pressurePa)
        .readBool(decoded.ok)
        .readStringView(decodedName);

    assert(reader.ok());
    assert(decoded.ms == original.ms);
    assert(decoded.temperatureC == original.temperatureC);
    assert(nearlyEqual(decoded.pressurePa, original.pressurePa));
    assert(decoded.ok == original.ok);
    assert(decodedName == originalName);

    // -------------------------------------------------------------------------
    // Framed message with header and payload checksums
    // -------------------------------------------------------------------------

    std::byte frame[256]{};
    pbook::MutableByteView frameBuffer(frame, sizeof(frame));

    std::byte payloadStorage[64]{};
    pbook::MutableByteView payloadBuffer(
        payloadStorage,
        sizeof(payloadStorage));

    weather::BinaryWriteStream payloadWriter(
        payloadBuffer,
        payloadWireEndianness);

    payloadWriter.writeUInt16(42)
        .writeUInt32(777)
        .writeString("opaque");

    assert(payloadWriter.ok());

    const auto payloadSize =
        static_cast<std::uint32_t>(payloadWriter.bytesWritten());

    const std::size_t headerSize = sizeof(MessageHeaderV1);

    // Reserve room for the header, then append the encoded payload.
    {
        weather::BinaryWriteStream frameWriter(
            frameBuffer,
            HeaderWireEndianness);

        const MessageHeaderV1 zeroHeader{};
        const auto* zeroHeaderBytes =
            reinterpret_cast<const std::byte*>(&zeroHeader);

        frameWriter.writeBytes(
            pbook::ImmutableByteView(
                zeroHeaderBytes,
                sizeof(zeroHeader)));

        frameWriter.writeBytes(
            pbook::ImmutableByteView(
                payloadStorage,
                payloadSize));

        assert(frameWriter.ok());
    }

    const pbook::ImmutableByteView framePayload(
        frame + headerSize,
        payloadSize);

    MessageHeaderV1 header{};
    header.version = 1;
    header.headerSize =
        static_cast<std::uint8_t>(sizeof(MessageHeaderV1));
    header.payloadEndian =
        static_cast<std::uint8_t>(
            payloadWireEndianness == Endianness::Little ? 0 : 1);
    header.headerFlags = 0;
    header.serviceId = 1;
    header.messageType = 9;
    header.payloadSize = payloadSize;
    header.flags = 0;
    header.reserved = 0;

    finalizeChecksums(header, framePayload);

    // Replace the reserved header bytes with the completed header.
    {
        pbook::MutableByteView headerRegion =
            subview(frameBuffer, 0, headerSize);

        weather::BinaryWriteStream headerWriter(
            headerRegion,
            HeaderWireEndianness);

        writeHeaderV1(headerWriter, header);
        assert(headerWriter.ok());
    }

    // Read and validate the complete frame.
    const pbook::ImmutableByteView frameView(
        frame,
        headerSize + payloadSize);

    weather::BinaryReadStream frameReader(
        frameView,
        HeaderWireEndianness);

    MessageHeaderV1 receivedHeader{};
    readHeaderV1(frameReader, receivedHeader);

    assert(frameReader.ok());
    assert(validateHeaderV1(receivedHeader));

    pbook::ImmutableByteView receivedPayload;
    frameReader.readBytesView(
        receivedHeader.payloadSize,
        receivedPayload);

    assert(frameReader.ok());
    assert(validatePayloadChecksum(
        receivedHeader,
        receivedPayload));

    // Decode the payload using the byte order declared in the header.
    const Endianness receivedPayloadEndianness =
        payloadEndianFromHeader(receivedHeader.payloadEndian);

    weather::BinaryReadStream payloadReader(
        receivedPayload,
        receivedPayloadEndianness);

    std::uint16_t firstValue{};
    std::uint32_t secondValue{};
    std::string_view text;

    payloadReader.readUInt16(firstValue)
        .readUInt32(secondValue)
        .readStringView(text);

    assert(payloadReader.ok());
    assert(firstValue == 42);
    assert(secondValue == 777);
    assert(text == "opaque");

    // Corrupt one payload byte and verify that validation fails.
    frame[headerSize + 1] ^= std::byte{0x01};

    const pbook::ImmutableByteView corruptedPayload(
        frame + headerSize,
        payloadSize);

    assert(!validatePayloadChecksum(
        receivedHeader,
        corruptedPayload));

    std::cout << "All BinaryDataStream demo tests passed.\n";
    return 0;
}
