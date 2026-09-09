// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "ParameterDefinitions.h"

namespace ch16
{

inline constexpr std::size_t ParameterTextCapacity = 64;

struct ParameterUpdateRequest
{
    ParameterID id{};
    ParameterSource source{};
    std::array<char, ParameterTextCapacity> value{};
    std::size_t valueLength{};

    [[nodiscard]] std::string_view text() const noexcept
    {
        return {value.data(), valueLength};
    }
};

inline std::optional<ParameterUpdateRequest> makeUpdateRequest(
    ParameterID id,
    std::string_view text,
    ParameterSource source) noexcept
{
    if (text.size() > ParameterTextCapacity)
    {
        return std::nullopt;
    }

    ParameterUpdateRequest request{};
    request.id = id;
    request.source = source;
    request.valueLength = text.size();
    std::copy(text.begin(), text.end(), request.value.begin());
    return request;
}

} // namespace ch16
