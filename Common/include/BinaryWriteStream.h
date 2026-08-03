// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Wilson
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ImmutableByteView.h"
#include "MutableByteView.h"
#include "BdsCommon.h"

namespace weather
{

class BinaryWriteStream
{
public:
    explicit BinaryWriteStream(MutableByteView buffer,
                               Endianness wireEndianness = Endianness::Little,
                               uint32_t maxSizedField = 0x00FFFFFFu) noexcept;

    bool ok() const noexcept;
    StreamError error() const noexcept;

    std::size_t bytesWritten() const noexcept;
    std::size_t remaining() const noexcept;

    // ---- Primitive writes (chaining) ----

    BinaryWriteStream& writeUInt8(uint8_t v) noexcept;
    BinaryWriteStream& writeInt8(int8_t v) noexcept;

    BinaryWriteStream& writeUInt16(uint16_t v) noexcept;
    BinaryWriteStream& writeInt16(int16_t v) noexcept;

    BinaryWriteStream& writeUInt32(uint32_t v) noexcept;
    BinaryWriteStream& writeInt32(int32_t v) noexcept;

    BinaryWriteStream& writeUInt64(uint64_t v) noexcept;
    BinaryWriteStream& writeInt64(int64_t v) noexcept;

    BinaryWriteStream& writeBool(bool v) noexcept;
    BinaryWriteStream& writeFloat(float v) noexcept;
    BinaryWriteStream& writeDouble(double v) noexcept;

    // ---- Sized field encoding (size optimization) ----
    BinaryWriteStream& writeSize(uint32_t n) noexcept;

    BinaryWriteStream& writeBytes(ImmutableByteView bytes) noexcept;

    BinaryWriteStream& writeString(std::string_view s) noexcept;

private:
    template <typename T>
    BinaryWriteStream& writeScalar(const T& value) noexcept;

    BinaryWriteStream& writeRawBytes(const void* data, std::size_t n) noexcept;

    bool ensureCapacity(std::size_t n) noexcept;

private:
    MutableByteView m_buf;
    std::size_t m_pos;
    StreamError m_err;

    Endianness m_host;
    Endianness m_wire;
    bool m_swap;

    uint32_t m_maxSizedField;
};

} // namespace weather

