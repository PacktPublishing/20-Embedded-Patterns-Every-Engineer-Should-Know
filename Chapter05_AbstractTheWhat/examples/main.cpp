#include <array>
#include <cstddef>
#include <iostream>

#include <ImagingCapabilities/AdjustIntegration.h>
#include <ImagingCapabilities/Defaults.h>

using namespace ImagingCapabilities;

class FixedImageSource : public IImageSource
{
public:
    std::string name() const override { return "fixed image source"; }
    std::string info() const override { return "test source for Chapter 5 examples"; }

    ImageBlock acquireImage() override
    {
        std::array<std::byte, 16> pixels{};
        pixels.fill(std::byte{80});
        return ImageBlock::copyFrom(4, 4, PixelFormat::GrayU8, pixels);
    }

    void startAcquisition(int) override {}
    ImageBlock waitForImage() override { return acquireImage(); }
};

int main()
{
    FixedImageSource source;
    RangeIntegrationControl rangeIntegration{Range<int>{100, 10000}, 1000};
    DiscreteIntegrationControl discreteIntegration{{100, 250, 500, 1000, 2000, 5000}};

    adjustIntegration(source, rangeIntegration, 128.0, 250);
    adjustIntegration(source, discreteIntegration, 128.0);

    std::cout << "range integration: " << rangeIntegration.integrationTimeUs() << " us\n";
    std::cout << "discrete integration: " << discreteIntegration.integrationTimeUs() << " us\n";
}
