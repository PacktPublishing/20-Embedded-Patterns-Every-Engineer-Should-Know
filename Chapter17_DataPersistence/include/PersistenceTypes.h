// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "ParameterDefinitions.h"

namespace ch17
{

inline constexpr std::size_t MaxStorageImageSize = 1024;
inline constexpr std::size_t StorageImageHeaderWireSize = 10;
inline constexpr std::uint16_t StorageImageVersion = 1;
inline constexpr std::uint16_t ParameterPersistenceServiceId = 0x0017;

inline constexpr std::array<std::byte, 4> StorageImageMagic{
    std::byte{0x50}, // P
    std::byte{0x49}, // I
    std::byte{0x4d}, // M
    std::byte{0x47}  // G
};

enum class ParameterMessageType : std::uint16_t
{
    UInt32 = 1,
    Float32 = 2,
    Enum8 = 3,
    Ascii16 = 16,
    Ascii32 = 17,
    Ascii255 = 18
};

enum class PersistenceStatus : std::uint8_t
{
    Ok,
    ImageNotFound,
    OpenFailed,
    ReadFailed,
    WriteFailed,
    ReplaceFailed,
    ImageFull,
    InvalidImageMagic,
    UnsupportedImageVersion,
    InvalidImageHeader,
    InvalidRecordCount,
    UnknownParameter,
    DuplicateParameter,
    InvalidBdsMessage,
    TypeMismatch,
    InvalidValue
};

struct PersistenceResult
{
    PersistenceStatus status{PersistenceStatus::Ok};
    std::size_t recordsProcessed{};
    std::optional<ch16::ParameterID> parameterId{};

    [[nodiscard]] constexpr bool succeeded() const noexcept
    {
        return status == PersistenceStatus::Ok;
    }
};

struct StorageImageHeader
{
    std::array<std::byte, 4> magic{StorageImageMagic};
    std::uint16_t version{StorageImageVersion};
    std::uint16_t recordCount{};
    std::uint16_t headerCrc{};
};

struct ParameterRecordView
{
    ch16::ParameterID parameterId{};
    std::span<const std::byte> bdsMessage{};
};

enum class WriteFault : std::uint8_t
{
    None,
    BeforeRename
};

enum class Corruption : std::uint8_t
{
    BadMagic,
    BadVersion,
    BadHeaderCrc,
    Truncate,
    BadPayload,
    UnknownParameter,
    DuplicateParameter,
    TypeMismatch,
    InvalidValue
};

} // namespace ch17
