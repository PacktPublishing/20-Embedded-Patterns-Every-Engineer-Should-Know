// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "ParameterDefinitions.h"
#include "ParameterStore.h"

namespace ch16
{

struct Handler
{
    ParameterID id;
    std::string_view name;
    std::size_t valueSize;

    bool (*validate)(const void*);
    bool (*parse)(std::string_view, void*);
    std::string_view (*serialize)(const void*, std::span<char>);
    UpdateResult (*setFromText)(
        ParameterStore&,
        std::string_view,
        ParameterSource,
        ParameterChangeEvent*);
};

template <typename Parameter>
UpdateResult setFromText(
    ParameterStore& store,
    std::string_view text,
    ParameterSource source,
    ParameterChangeEvent* event) noexcept
{
    typename Parameter::ValueType value{};
    if (!Parameter::parse(text, value))
    {
        return UpdateResult::INVALID_VALUE;
    }
    return store.set<Parameter>(value, source, event);
}

template <typename Parameter>
constexpr Handler makeHandler() noexcept
{
    using ValueType = typename Parameter::ValueType;

    return Handler{
        Parameter::id,
        Parameter::name,
        sizeof(ValueType),

        [](const void* value) -> bool
        {
            return Parameter::validate(*static_cast<const ValueType*>(value));
        },

        [](std::string_view text, void* value) -> bool
        {
            return Parameter::parse(text, *static_cast<ValueType*>(value));
        },

        [](const void* value, std::span<char> buffer) -> std::string_view
        {
            return Parameter::serialize(
                *static_cast<const ValueType*>(value), buffer);
        },

        &setFromText<Parameter>};
}

inline constexpr std::array<Handler, parameterCount> registry{
    makeHandler<ReportingInterval>(),
    makeHandler<TemperatureLimit>(),
    makeHandler<OperatingModeParameter>()};

constexpr const Handler& handlerFor(ParameterID id) noexcept
{
    return registry[toIndex(id)];
}

static_assert(handlerFor(ParameterID::ReportingInterval).id ==
              ParameterID::ReportingInterval);
static_assert(handlerFor(ParameterID::TemperatureLimit).id ==
              ParameterID::TemperatureLimit);
static_assert(handlerFor(ParameterID::OperatingMode).id ==
              ParameterID::OperatingMode);

// Text-name translation is intentionally an interface-boundary operation.
// The core registry is indexed directly by ParameterID.
inline std::optional<ParameterID> parameterIdFromName(std::string_view name) noexcept
{
    for (const auto& handler : registry)
    {
        if (handler.name == name)
        {
            return handler.id;
        }
    }
    return std::nullopt;
}

inline UpdateResult ParameterStore::set(
    ParameterID id,
    std::string_view text,
    ParameterSource source,
    ParameterChangeEvent* event) noexcept
{
    const Handler& handler = handlerFor(id);
    return handler.setFromText(*this, text, source, event);
}

} // namespace ch16
