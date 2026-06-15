#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>

namespace ImagingCapabilities
{

enum class PixelFormat
{
    GrayU8,
    GrayU16,
    GrayS16,
    GrayU32,
    GrayF32
};

inline constexpr std::size_t bytesPerPixel(PixelFormat format) noexcept
{
    switch (format)
    {
        case PixelFormat::GrayU8:  return 1;
        case PixelFormat::GrayU16: return 2;
        case PixelFormat::GrayS16: return 2;
        case PixelFormat::GrayU32: return 4;
        case PixelFormat::GrayF32: return 4;
    }

    return 0;
}

inline constexpr std::string_view pixelFormatName(PixelFormat format) noexcept
{
    switch (format)
    {
        case PixelFormat::GrayU8:  return "GrayU8";
        case PixelFormat::GrayU16: return "GrayU16";
        case PixelFormat::GrayS16: return "GrayS16";
        case PixelFormat::GrayU32: return "GrayU32";
        case PixelFormat::GrayF32: return "GrayF32";
    }

    return "Unknown";
}

// ImageBlock manages image data. The data members reflect the settings in
// effect on the image source at the time the data was acquired.
//
// It is not intended to be a full image model. It is a small value-semantics
// wrapper used to decouple acquired image data from the source that produced it.
// The source may be a camera, frame grabber, file replay, simulator, or test
// double.
//
// ImageBlock objects are cheap to copy. Copying an ImageBlock shares the same
// underlying image data using std::shared_ptr. If an independent copy is needed,
// call clone().
//
// Most ImageBlock instances own heap-allocated data. The fromExternal() factory
// exists for data owned by another subsystem, such as a DMA buffer, frame grabber
// ring buffer, simulator, or vendor API. The custom deleter may free memory,
// return it to a pool, or do nothing at all.
class ImageBlock final
{
public:
    using Deleter = std::function<void(const std::byte*)>;

    ImageBlock() noexcept = default;

    static ImageBlock make(std::size_t width, std::size_t height, PixelFormat format)
    {
        if (width == 0 || height == 0)
            return {};

        const auto rowBytes = checkedMultiply(width, ImagingCapabilities::bytesPerPixel(format));
        return makeWithStride(width, height, format, rowBytes);
    }

    static ImageBlock makeWithStride(
        std::size_t width,
        std::size_t height,
        PixelFormat format,
        std::size_t strideBytes)
    {
        if (width == 0 || height == 0)
            return {};

        const auto minimumStride = checkedMultiply(width, ImagingCapabilities::bytesPerPixel(format));
        if (strideBytes < minimumStride)
            throw std::invalid_argument{"ImageBlock stride is smaller than one image row"};

        const auto allocationSize = checkedMultiply(strideBytes, height);
        auto raw = std::shared_ptr<std::byte[]>{new std::byte[allocationSize]{}, std::default_delete<std::byte[]>{}};
        std::shared_ptr<const std::byte> view{raw, raw.get()};

        return ImageBlock{width, height, strideBytes, format, std::move(view)};
    }

    static ImageBlock copyFrom(
        std::size_t width,
        std::size_t height,
        PixelFormat format,
        std::span<const std::byte> contiguousData)
    {
        auto block = make(width, height, format);
        if (!block)
            return {};

        if (contiguousData.size() < block.rowBytes() * block.height())
            throw std::invalid_argument{"ImageBlock copy source is too small"};

        std::byte* destination = const_cast<std::byte*>(block.data());
        std::memcpy(destination, contiguousData.data(), block.rowBytes() * block.height());
        return block;
    }

    static ImageBlock fromExternal(
        std::size_t width,
        std::size_t height,
        PixelFormat format,
        std::size_t strideBytes,
        const std::byte* data,
        Deleter deleter)
    {
        if (width == 0 || height == 0)
            return {};

        if (data == nullptr)
            throw std::invalid_argument{"ImageBlock external data pointer may not be null"};

        const auto minimumStride = checkedMultiply(width, ImagingCapabilities::bytesPerPixel(format));
        if (strideBytes < minimumStride)
            throw std::invalid_argument{"ImageBlock stride is smaller than one image row"};

        return ImageBlock{width, height, strideBytes, format, std::shared_ptr<const std::byte>{data, std::move(deleter)}};
    }

    ImageBlock clone() const
    {
        if (!*this)
            return {};

        auto copy = makeWithStride(width_, height_, format_, strideBytes_);
        copyTo(copy);
        return copy;
    }

    ImageBlock(const ImageBlock&) noexcept = default;
    ImageBlock(ImageBlock&&) noexcept = default;
    ImageBlock& operator=(const ImageBlock&) noexcept = default;
    ImageBlock& operator=(ImageBlock&&) noexcept = default;
    ~ImageBlock() = default;

    explicit operator bool() const noexcept { return data_ != nullptr; }

    std::size_t width() const noexcept { return width_; }
    std::size_t height() const noexcept { return height_; }
    std::size_t strideBytes() const noexcept { return strideBytes_; }
    PixelFormat pixelFormat() const noexcept { return format_; }
    PixelFormat format() const noexcept { return format_; }
    std::size_t bytesPerPixel() const noexcept { return ImagingCapabilities::bytesPerPixel(format_); }
    std::size_t rowBytes() const noexcept { return width_ * bytesPerPixel(); }
    std::size_t sizeBytes() const noexcept { return strideBytes_ * height_; }
    std::size_t numberOfPixels() const noexcept { return width_ * height_; }

    const std::byte* data() const noexcept { return data_.get(); }

    std::span<const std::byte> bytes() const noexcept
    {
        return data_ ? std::span<const std::byte>{data_.get(), sizeBytes()} : std::span<const std::byte>{};
    }

    std::span<const std::byte> row(std::size_t rowIndex) const
    {
        if (rowIndex >= height_)
            throw std::out_of_range{"ImageBlock row index out of range"};

        return {data_.get() + rowIndex * strideBytes_, rowBytes()};
    }

    const std::byte& operator[](std::size_t index) const
    {
        if (index >= sizeBytes())
            throw std::out_of_range{"ImageBlock byte index out of range"};

        return data_.get()[index];
    }

    void copyTo(ImageBlock& destination) const
    {
        if (!*this || !destination)
            return;

        if (destination.width_ != width_ || destination.height_ != height_ || destination.format_ != format_)
            throw std::invalid_argument{"ImageBlock copyTo size or format mismatch"};

        for (std::size_t y = 0; y < height_; ++y)
        {
            const auto* sourceRow = data_.get() + y * strideBytes_;
            auto* destinationRow = const_cast<std::byte*>(destination.data()) + y * destination.strideBytes_;
            std::memcpy(destinationRow, sourceRow, rowBytes());
        }
    }

private:
    ImageBlock(
        std::size_t width,
        std::size_t height,
        std::size_t strideBytes,
        PixelFormat format,
        std::shared_ptr<const std::byte> data) noexcept :
        width_{width},
        height_{height},
        strideBytes_{strideBytes},
        format_{format},
        data_{std::move(data)}
    {
    }

    static std::size_t checkedMultiply(std::size_t a, std::size_t b)
    {
        if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a)
            throw std::overflow_error{"ImageBlock size overflow"};

        return a * b;
    }

    std::size_t width_{0};
    std::size_t height_{0};
    std::size_t strideBytes_{0};
    PixelFormat format_{PixelFormat::GrayU8};
    std::shared_ptr<const std::byte> data_{};
};

struct NullImageBlockDeleter
{
    void operator()(const std::byte*) const noexcept
    {
        // Used when the memory is owned by a ring buffer, frame grabber API,
        // simulator, or other subsystem that manages the lifetime externally.
    }
};

} // namespace ImagingCapabilities
