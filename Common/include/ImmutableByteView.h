// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * @brief Immutable, non-owning view of a contiguous byte sequence.
 *
 * ImmutableByteView does not own the memory it references. It is a lightweight
 * `(pointer, size)` wrapper intended to express read-only access to
 * arbitrary binary data.
 *
 * This type is conceptually similar to `std::span<const std::byte>`,
 * but is available in C++17.
 *
 * Lifetime:
 *  - The caller is responsible for ensuring the referenced memory
 *    remains valid for the lifetime of the ImmutableByteView.
 *
 * Thread safety:
 *  - ImmutableByteView itself is trivially copyable and thread-safe.
 *  - The underlying memory is not synchronized.
 */
class ImmutableByteView
{
public:
    /// Constructs an empty ImmutableByteView.
    ImmutableByteView() noexcept
        : mData(nullptr)
        , mSize(0)
    {
    }

    /**
     * @brief Constructs a ImmutableByteView from raw memory.
     *
     * @param data Pointer to the first byte (may be nullptr if size == 0).
     * @param size Number of bytes in the view.
     */
    ImmutableByteView(const void* data, std::size_t size) noexcept
        : mData(static_cast<const std::byte*>(data))
        , mSize(size)
    {
    }

    /// @return Pointer to the first byte (read-only).
    const std::byte* data() const noexcept { return mData; }

    /// @return Number of bytes in the view.
    std::size_t size() const noexcept { return mSize; }

    /// @return True if the view contains no bytes.
    bool empty() const noexcept { return mSize == 0; }

    /// Read-only indexed access (no bounds checking).
    const std::byte& operator[](std::size_t i) const noexcept { return mData[i]; }

private:
    const std::byte* mData;
    std::size_t mSize;
};

// -----------------------------------------------------------------------------
// Convenience helpers.
// -----------------------------------------------------------------------------

/// Create an immutable ImmutableByteView from raw memory.
inline ImmutableByteView asBytes(const void* data, std::size_t size) noexcept
{
    return ImmutableByteView(data, size);
}

/// Create an immutable ByteView from a std::array.
template<typename T, std::size_t N>
inline ImmutableByteView asBytes(const std::array<T, N>& a) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "asBytes(std::array<...>) requires a trivial element type");
    return ImmutableByteView(a.data(), N * sizeof(T));
}
