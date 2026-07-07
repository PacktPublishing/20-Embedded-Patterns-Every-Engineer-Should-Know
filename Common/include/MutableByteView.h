// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#pragma once

#include "ImmutableByteView.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

namespace pbook
{

/**
 * @brief Mutable, non-owning view of a contiguous byte sequence.
 *
 * MutableByteView expresses intent to modify the underlying memory. It is an
 * alias for std::span<std::byte>.
 *
 * A MutableByteView is implicitly convertible to ImmutableByteView through
 * std::span's normal conversion from std::span<T> to std::span<const T>.
 */
using MutableByteView = std::span<std::byte>;

// -----------------------------------------------------------------------------
// Convenience helpers.
// -----------------------------------------------------------------------------

/// Create a mutable byte view from raw memory.
inline MutableByteView asWritableBytes(void* data, std::size_t size) noexcept
{
    return MutableByteView{static_cast<std::byte*>(data), size};
}

/// Create a mutable byte view from a std::array.
template<typename T, std::size_t N>
inline MutableByteView asWritableBytes(std::array<T, N>& a) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "asWritableBytes(std::array<...>) requires a trivial element type");

    return std::as_writable_bytes(std::span{a});
}

} // end namespace
