// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#pragma once

#include <cstddef>
#include <cstdint>

#include "ImmutableByteView.h"
#include "MutableByteView.h"

//------------------------------------------------------------------------------
// Helpers for working with byte-view buffers
//------------------------------------------------------------------------------

std::uint8_t* u8(pbook::MutableByteView bytes) noexcept;
const std::uint8_t* u8(pbook::ImmutableByteView bytes) noexcept;

pbook::MutableByteView subview(
    pbook::MutableByteView bytes,
    std::size_t offset,
    std::size_t length) noexcept;

pbook::ImmutableByteView subview(
    pbook::ImmutableByteView bytes,
    std::size_t offset,
    std::size_t length) noexcept;

//------------------------------------------------------------------------------
// Endianness
//------------------------------------------------------------------------------

enum class Endianness : std::uint8_t
{
    Little = 0,
    Big = 1
};

Endianness detectHostEndianness() noexcept;

//------------------------------------------------------------------------------
// Stream error (embedded-friendly, latched)
//------------------------------------------------------------------------------

enum class StreamError : std::uint8_t
{
    None = 0,
    BufferOverflow,
    BufferUnderflow,
    SizeLimitExceeded,
    InvalidData
};

//------------------------------------------------------------------------------
// Safe copying with optional byte reversal
//------------------------------------------------------------------------------

void copyForward(
    std::uint8_t* destination,
    const std::uint8_t* source,
    std::size_t size) noexcept;

void copyReversed(
    std::uint8_t* destination,
    const std::uint8_t* source,
    std::size_t size) noexcept;

//------------------------------------------------------------------------------
// CRC-16/CCITT-FALSE
//------------------------------------------------------------------------------
//
// Parameters:
//
//   Polynomial:   0x1021
//   Initial value: 0xFFFF
//   Reflect input: false
//   Reflect output: false
//   Final XOR:     0x0000
//
// The standard check value for the ASCII bytes "123456789" is 0x29B1.
//
// CRC detects accidental corruption. It does not provide security,
// authenticity, or protection against deliberate modification.
//------------------------------------------------------------------------------

inline constexpr std::uint16_t Crc16CcittPolynomial = 0x1021u;
inline constexpr std::uint16_t Crc16CcittInitialValue = 0xFFFFu;

std::uint16_t crc16CcittFalse(
    const std::uint8_t* data,
    std::size_t size) noexcept;

std::uint16_t crc16CcittFalse(
    pbook::ImmutableByteView bytes) noexcept;
