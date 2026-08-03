// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Wilson
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ImmutableByteView.h"
#include "BdsCommon.h"

namespace weather
{

class BinaryReadStream
{
public:
    explicit BinaryReadStream(pbook::ImmutableByteView buffer,
                              Endianness wireEndianness = Endianness::Little,
                              uint32_t maxSizedField = 0x00FFFFFFu) noexcept;

    bool ok() const noexcept;
    StreamError error() const noexcept;

    std::size_t bytesRead() const noexcept;
    std::size_t remaining() const noexcept;

    // ---- Primitive reads (chaining) ----
    BinaryReadStream& readUInt8(uint8_t& out) noexcept;
    BinaryReadStream& readInt8(int8_t& out) noexcept;

    BinaryReadStream& readUInt16(uint16_t& out) noexcept;
    BinaryReadStream& readInt16(int16_t& out) noexcept;

    BinaryReadStream& readUInt32(uint32_t& out) noexcept;
    BinaryReadStream& readInt32(int32_t& out) noexcept;

    BinaryReadStream& readUInt64(uint64_t& out) noexcept;
    BinaryReadStream& readInt64(int64_t& out) noexcept;

    BinaryReadStream& readBool(bool& out) noexcept;
    BinaryReadStream& readFloat(float& out) noexcept;
    BinaryReadStream& readDouble(double& out) noexcept;

    // ---- Size decoding ----
    BinaryReadStream& readSize(uint32_t& out) noexcept;

    // Pass-through view (no decoding)
    BinaryReadStream& readBytesView(uint32_t n, pbook::ImmutableByteView& outView) noexcept;

    BinaryReadStream& readStringView(std::string_view& out) noexcept;

private:
    template <typename T>
    BinaryReadStream& readScalar(T& out) noexcept;

    bool ensureAvailable(std::size_t n) noexcept;

private:
    pbook::ImmutableByteView m_buf;
    std::size_t m_pos;
    StreamError m_err;

    Endianness m_host;
    Endianness m_wire;
    bool m_swap;

    uint32_t m_maxSizedField;
};

} // namespace weather
