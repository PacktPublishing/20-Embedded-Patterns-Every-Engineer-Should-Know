// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
#include "ImmutableByteView.h"
#include "MutableByteView.h"
#include "ParameterDefinitions.h"

namespace ch16
{

struct ParameterChangeEvent
{
    ParameterID id{};
    ParameterSource source{};
    std::uint32_t generation{};
};

class ParameterStore
{
public:
    ParameterStore()
    {
        sources_.fill(ParameterSource::CompiledDefault);
        initializeDefault<ReportingInterval>();
        initializeDefault<TemperatureLimit>();
        initializeDefault<OperatingModeParameter>();
    }

    template <typename Parameter>
    bool get(typename Parameter::ValueType& value) const noexcept
    {
        static_assert(
            Parameter::offset + Parameter::encodedSize <= ParameterImageSize,
            "Parameter extends beyond parameter image");

        const pbook::ImmutableByteView bytes(
            values_.data() + Parameter::offset,
            Parameter::encodedSize);

        pbook::BinaryReadStream reader(bytes, ParameterImageEndianness);
        Parameter::decode(reader, value);
        return reader.ok();
    }

    template <typename Parameter>
    UpdateResult set(
        typename Parameter::ValueType value,
        ParameterSource source,
        ParameterChangeEvent* event = nullptr) noexcept
    {
        static_assert(
            Parameter::offset + Parameter::encodedSize <= ParameterImageSize,
            "Parameter extends beyond parameter image");

        if constexpr (!Parameter::writable)
        {
            return UpdateResult::READ_ONLY;
        }

        if (source == ParameterSource::Runtime && !Parameter::runtimeWritable)
        {
            return UpdateResult::NOT_RUNTIME_WRITABLE;
        }

        if (!Parameter::validate(value))
        {
            return UpdateResult::INVALID_VALUE;
        }

        const auto index = toIndex(Parameter::id);
        if (precedence(source) < precedence(sources_[index]))
        {
            return UpdateResult::LOWER_PRECEDENCE;
        }

        // Encode into temporary storage first. The live image is changed only
        // after the complete value has encoded successfully.
        std::array<std::byte, Parameter::encodedSize> encoded{};
        pbook::MutableByteView bytes(encoded.data(), encoded.size());
        pbook::BinaryWriteStream writer(bytes, ParameterImageEndianness);
        Parameter::encode(writer, value);

        if (!writer.ok() || writer.bytesWritten() != Parameter::encodedSize)
        {
            return UpdateResult::INVALID_VALUE;
        }

        std::copy(
            encoded.begin(), encoded.end(),
            values_.begin() + static_cast<std::ptrdiff_t>(Parameter::offset));

        sources_[index] = source;
        ++generation_;

        if (event != nullptr)
        {
            *event = ParameterChangeEvent{
                Parameter::id,
                source,
                generation_};
        }

        return UpdateResult::OK;
    }

    UpdateResult set(
        ParameterID id,
        std::string_view text,
        ParameterSource source,
        ParameterChangeEvent* event = nullptr) noexcept;

    [[nodiscard]] ParameterSource source(ParameterID id) const noexcept
    {
        return sources_[toIndex(id)];
    }

    [[nodiscard]] std::uint32_t generation() const noexcept
    {
        return generation_;
    }

    [[nodiscard]] pbook::ImmutableByteView image() const noexcept
    {
        return pbook::ImmutableByteView(values_.data(), values_.size());
    }

private:
    template <typename Parameter>
    void initializeDefault()
    {
        std::array<std::byte, Parameter::encodedSize> encoded{};
        pbook::MutableByteView bytes(encoded.data(), encoded.size());
        pbook::BinaryWriteStream writer(bytes, ParameterImageEndianness);
        Parameter::encode(writer, Parameter::defaultValue);

        if (!writer.ok() || writer.bytesWritten() != Parameter::encodedSize)
        {
            // The fixed default encodings are design-time invariants. There is
            // no useful recovery path here; leave the zero-initialized image.
            return;
        }

        std::copy(
            encoded.begin(), encoded.end(),
            values_.begin() + static_cast<std::ptrdiff_t>(Parameter::offset));
    }

    std::array<std::byte, ParameterImageSize> values_{};
    std::array<ParameterSource, parameterCount> sources_{};
    std::uint32_t generation_{0};
};

} // namespace ch16
