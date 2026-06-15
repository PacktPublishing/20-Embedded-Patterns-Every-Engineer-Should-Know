#pragma once

#include <cstdint>

namespace ImagingCapabilities
{

struct Dimensions
{
    int width{0};
    int height{0};

    constexpr bool operator==(const Dimensions& other) const noexcept
    {
        return width == other.width && height == other.height;
    }
};

struct Rect
{
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    constexpr bool operator==(const Rect& other) const noexcept
    {
        return x == other.x &&
               y == other.y &&
               width == other.width &&
               height == other.height;
    }
};

}
