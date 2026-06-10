// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#pragma once

#include "ImmutableByteView.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * @brief Mutable, non-owning view of a contiguous byte sequence.
 *
 * MutableByteView expresses intent to modify the underlying memory.
 * It is otherwise identical to ByteView in structure and lifetime
 * requirements.
 *
 * MutableByteView is implicitly convertible to ByteView to support
 * APIs that require read-only access.
 */
class MutableByteView
{
public:
    /// Constructs an empty MutableByteView.
    MutableByteView() noexcept
        : mData(nullptr)
        , mSize(0)
    {
    }

    /**
     * @brief Constructs a MutableByteView from raw memory.
     *
     * @param data Pointer to the first byte (may be nullptr if size == 0).
     * @param size Number of bytes in the view.
     */
    MutableByteView(void* data, std::size_t size) noexcept
        : mData(static_cast<std::byte*>(data))
        , mSize(size)
    {
    }

    /// @return Pointer to the first byte (mutable).
    std::byte* data() const noexcept { return mData; }

    /// @return Number of bytes in the view.
    std::size_t size() const noexcept { return mSize; }

    /// @return True if the view contains no bytes.
    bool empty() const noexcept { return mSize == 0; }

    /// Mutable indexed access (no bounds checking).
    std::byte& operator[](std::size_t i) const noexcept { return mData[i]; }

    /**
     * @brief Implicit conversion to an ImmutableByteView.
     *
     * Allows MutableByteView to be passed to read-only APIs
     * without copying or allocation.
     */
    operator ImmutableByteView() const noexcept
    {
        return ImmutableByteView(mData, mSize);
    }

private:
    std::byte* mData;
    std::size_t mSize;
};

// -----------------------------------------------------------------------------
// Convenience helpers.
// -----------------------------------------------------------------------------

/// Create a mutable ByteView from raw memory.
inline MutableByteView asWritableBytes(void* data, std::size_t size) noexcept
{
    return MutableByteView(data, size);
}

/// Create a mutable ByteView from a std::array.
template<typename T, std::size_t N>
inline MutableByteView asWritableBytes(std::array<T, N>& a) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "asWritableBytes(std::array<...>) requires a trivial element type");
    return MutableByteView(a.data(), N * sizeof(T));
}
