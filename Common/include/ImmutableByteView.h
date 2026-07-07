// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

namespace pbook
{

/**
 * @brief Immutable, non-owning view of a contiguous byte sequence.
 *
 * ImmutableByteView does not own the memory it references. It is an alias for
 * std::span<const std::byte>, which expresses read-only access to arbitrary
 * binary data.
 *
 * Lifetime:
 *  - The caller is responsible for ensuring the referenced memory remains valid
 *    for the lifetime of the ImmutableByteView.
 *
 * Thread safety:
 *  - ImmutableByteView is cheap to copy.
 *  - It provides no synchronization for the underlying memory.
 */
using ImmutableByteView = std::span<const std::byte>;

// -----------------------------------------------------------------------------
// Convenience helpers.
// -----------------------------------------------------------------------------

/// Create an immutable byte view from raw memory.
inline ImmutableByteView asBytes(const void* data, std::size_t size) noexcept
{
    return ImmutableByteView{static_cast<const std::byte*>(data), size};
}

/// Create an immutable byte view from a std::array.
template<typename T, std::size_t N>
inline ImmutableByteView asBytes(const std::array<T, N>& a) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "asBytes(std::array<...>) requires a trivial element type");

    return std::as_bytes(std::span{a});
}

} // end of namespace
