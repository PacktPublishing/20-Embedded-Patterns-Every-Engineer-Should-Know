// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <filesystem>
#include <span>
#include <string_view>

#include "ParameterStore.h"
#include "PersistenceTypes.h"

namespace ch17
{

class ParameterStorageImage
{
public:
    [[nodiscard]] PersistenceResult create(
        const ch16::ParameterStore& parameters) noexcept;

    [[nodiscard]] PersistenceResult update(
        ch16::ParameterID id,
        const ch16::ParameterStore& parameters) noexcept;

    [[nodiscard]] PersistenceResult restore(
        ch16::ParameterStore& parameters) const noexcept;

    [[nodiscard]] PersistenceResult assign(
        std::span<const std::byte> bytes) noexcept;

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept
    {
        return {data_.data(), size_};
    }

    [[nodiscard]] std::span<std::byte> mutableBytes() noexcept
    {
        return {data_.data(), size_};
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return size_;
    }

private:
    std::array<std::byte, MaxStorageImageSize> data_{};
    std::size_t size_{};
};

[[nodiscard]] PersistenceResult readImageFile(
    const std::filesystem::path& path,
    ParameterStorageImage& image) noexcept;

[[nodiscard]] PersistenceResult writeImageFileAtomically(
    const std::filesystem::path& path,
    std::span<const std::byte> immutableSnapshot,
    WriteFault fault = WriteFault::None) noexcept;

[[nodiscard]] PersistenceResult inspectImage(
    std::span<const std::byte> image,
    bool printRecords = true) noexcept;

[[nodiscard]] bool corruptImage(
    std::span<std::byte> image,
    Corruption mode) noexcept;

[[nodiscard]] const char* toString(PersistenceStatus status) noexcept;
[[nodiscard]] const char* toString(ch16::ParameterID id) noexcept;
[[nodiscard]] const char* toString(ParameterMessageType type) noexcept;
[[nodiscard]] std::optional<Corruption> parseCorruption(
    std::string_view text) noexcept;

void printParameters(const ch16::ParameterStore& parameters);

} // namespace ch17
