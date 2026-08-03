// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Wilson

#pragma once

#include <cstddef>
#include <cstdint>

#include "ImmutableByteView.h"
#include "MutableByteView.h"

//------------------------------------------------------------------------------
// Helpers for working with byte-view buffers
//------------------------------------------------------------------------------

uint8_t* u8(MutableByteView b) noexcept;
const uint8_t* u8(ImmutableByteView b) noexcept;

MutableByteView subview(MutableByteView b, std::size_t offset, std::size_t len) noexcept;
ImmutableByteView subview(ImmutableByteView b, std::size_t offset, std::size_t len) noexcept;

//------------------------------------------------------------------------------
// Endianness
//------------------------------------------------------------------------------

enum class Endianness : uint8_t
{
    Little = 0,
    Big = 1
};

Endianness detectHostEndianness() noexcept;

//------------------------------------------------------------------------------
// Stream error (embedded-friendly, latched)
//------------------------------------------------------------------------------

enum class StreamError : uint8_t
{
    None = 0,
    BufferOverflow,
    BufferUnderflow,
    SizeLimitExceeded,
    InvalidData
};

//------------------------------------------------------------------------------
// Utility: safe copy with optional byte reversal
//------------------------------------------------------------------------------

void copyForward(uint8_t* dst, const uint8_t* src, std::size_t n) noexcept;
void copyReversed(uint8_t* dst, const uint8_t* src, std::size_t n) noexcept;

//------------------------------------------------------------------------------
// Rolling XOR checksum (corruption detection, not security)
//------------------------------------------------------------------------------

uint8_t xorChecksum(const uint8_t* data, std::size_t n) noexcept;
uint8_t xorChecksum(ImmutableByteView b) noexcept;

