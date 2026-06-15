#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <numeric>
#include <stdexcept>

#include "IImageSource.h"
#include "IIntegration.h"

namespace ImagingCapabilities
{

struct MeanIntensity
{
    double operator()(const ImageBlock& image) const
    {
        if (!image || image.sizeBytes() == 0)
            throw std::invalid_argument{"cannot compute statistics for empty image data"};

        const auto bytes = image.bytes();
        const auto total = std::accumulate(
            bytes.begin(),
            bytes.end(),
            std::uint64_t{0},
            [](std::uint64_t sum, std::byte value)
            {
                return sum + std::to_integer<unsigned int>(value);
            });

        return static_cast<double>(total) / static_cast<double>(bytes.size());
    }
};

// Algorithm written against the capabilities it needs, not against a camera type.
// This version works with devices that expose integration time as a range.
template <typename Statistics = MeanIntensity>
void adjustIntegration(
    IImageSource& source,
    IIntegrationTimeRange& integration,
    double targetMean,
    int stepUs,
    Statistics statistics = {})
{
    const auto image = source.acquireImage();
    const auto mean = statistics(image);

    auto next = integration.integrationTimeUs();
    if (mean < targetMean)
        next += stepUs;
    else if (mean > targetMean)
        next -= stepUs;

    const auto range = integration.integrationTimeRangeUs();
    next = std::clamp(next, range.min(), range.max());

    if (integration.isIntegrationTimeValidUs(next))
        integration.setIntegrationTimeUs(next);
}

// Same algorithmic intent, but written against a discrete integration capability.
template <typename Statistics = MeanIntensity>
void adjustIntegration(
    IImageSource& source,
    IIntegrationTimeDiscrete& integration,
    double targetMean,
    Statistics statistics = {})
{
    const auto image = source.acquireImage();
    const auto mean = statistics(image);

    auto values = integration.validIntegrationTimesUs();
    if (values.empty())
        return;

    std::sort(values.begin(), values.end());

    auto current = integration.integrationTimeUs();
    auto position = std::lower_bound(values.begin(), values.end(), current);
    if (position == values.end())
        position = std::prev(values.end());

    if (mean < targetMean && std::next(position) != values.end())
        integration.setIntegrationTimeUs(*std::next(position));
    else if (mean > targetMean && position != values.begin())
        integration.setIntegrationTimeUs(*std::prev(position));
}

}
