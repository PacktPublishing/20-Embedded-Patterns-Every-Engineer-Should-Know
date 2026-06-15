#pragma once

#include <stdexcept>
#include <string>

#include "ImageBlock.h"

namespace ImagingCapabilities
{

class ImageAcquisitionFailed : public std::runtime_error
{
public:
    explicit ImageAcquisitionFailed(const std::string& reason) :
        std::runtime_error{"image acquisition failed: " + reason}
    {
    }
};

// Capability interface for obtaining image data. The source may be a camera,
// frame grabber, replay file, directory of image files, simulator, or test double.
class IImageSource
{
public:
    static constexpr int infinite = -1;

    virtual ~IImageSource() = default;

    virtual std::string name() const = 0;
    virtual std::string info() const = 0;

    // Synchronously acquire image data. The returned ImageBlock describes the
    // data actually acquired, which matters when a source can produce varying
    // dimensions, pixel formats, stride, or other acquisition-time characteristics.
    virtual ImageBlock acquireImage() = 0;

    // Optional asynchronous acquisition shape. Simple implementations may throw
    // or provide a default wrapper around acquireImage().
    virtual void startAcquisition(int imageCount) = 0;
    virtual ImageBlock waitForImage() = 0;
};

}
