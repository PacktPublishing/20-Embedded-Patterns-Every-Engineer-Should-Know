#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "IAreaOfInterest.h"
#include "IBinning.h"
#include "IGain.h"
#include "IImageSource.h"
#include "IIntegration.h"
#include "IOffset.h"

namespace ImagingCapabilities
{

class NullImageSource : public IImageSource
{
public:
    std::string name() const override { return "null image source"; }
    std::string info() const override { return "no image source configured"; }
    ImageBlock acquireImage() override { return {}; }
    void startAcquisition(int) override {}
    ImageBlock waitForImage() override { return {}; }
};

class RangeIntegrationControl : public IIntegrationModeControl, public IIntegrationTimeRange
{
public:
    RangeIntegrationControl(Range<int> integrationRange, int initialTimeUs) :
        integrationRange_{integrationRange},
        integrationTimeUs_{initialTimeUs}
    {
        if (!contains(integrationRange_, initialTimeUs))
            throw std::out_of_range{"initial integration time is outside supported range"};
    }

    void setIntegrationMode(IntegrationMode mode) override
    {
        if (!isIntegrationModeValid(mode))
            throw std::out_of_range{"unsupported integration mode"};
        mode_ = mode;
    }

    IntegrationMode integrationMode() const override { return mode_; }

    IntegrationModeList validIntegrationModes() const override
    {
        return {IntegrationMode::FreeRun, IntegrationMode::ProgrammableHardware};
    }

    bool isIntegrationModeValid(IntegrationMode mode) const override
    {
        const auto modes = validIntegrationModes();
        return std::find(modes.begin(), modes.end(), mode) != modes.end();
    }

    void setIntegrationTimeUs(int microseconds) override
    {
        if (!isIntegrationTimeValidUs(microseconds))
            throw std::out_of_range{"unsupported integration time"};
        integrationTimeUs_ = microseconds;
    }

    int integrationTimeUs() const override { return integrationTimeUs_; }
    Range<int> integrationTimeRangeUs() const override { return integrationRange_; }
    bool isIntegrationTimeValidUs(int microseconds) const override
    {
        return contains(integrationRange_, microseconds);
    }

private:
    IntegrationMode mode_{IntegrationMode::FreeRun};
    Range<int> integrationRange_{};
    int integrationTimeUs_{};
};

class DiscreteIntegrationControl : public IIntegrationModeControl, public IIntegrationTimeDiscrete
{
public:
    explicit DiscreteIntegrationControl(std::vector<int> valuesUs) :
        valuesUs_{std::move(valuesUs)}
    {
        if (valuesUs_.empty())
            throw std::invalid_argument{"discrete integration capability needs at least one value"};
        std::sort(valuesUs_.begin(), valuesUs_.end());
        integrationTimeUs_ = valuesUs_.front();
    }

    void setIntegrationMode(IntegrationMode mode) override
    {
        if (!isIntegrationModeValid(mode))
            throw std::out_of_range{"unsupported integration mode"};
        mode_ = mode;
    }

    IntegrationMode integrationMode() const override { return mode_; }

    IntegrationModeList validIntegrationModes() const override
    {
        return {IntegrationMode::FreeRun, IntegrationMode::ExternalHardware};
    }

    bool isIntegrationModeValid(IntegrationMode mode) const override
    {
        const auto modes = validIntegrationModes();
        return std::find(modes.begin(), modes.end(), mode) != modes.end();
    }

    void setIntegrationTimeUs(int microseconds) override
    {
        if (!isIntegrationTimeValidUs(microseconds))
            throw std::out_of_range{"unsupported integration time"};
        integrationTimeUs_ = microseconds;
    }

    int integrationTimeUs() const override { return integrationTimeUs_; }
    std::vector<int> validIntegrationTimesUs() const override { return valuesUs_; }
    bool isIntegrationTimeValidUs(int microseconds) const override
    {
        return std::find(valuesUs_.begin(), valuesUs_.end(), microseconds) != valuesUs_.end();
    }

private:
    IntegrationMode mode_{IntegrationMode::FreeRun};
    std::vector<int> valuesUs_{};
    int integrationTimeUs_{};
};

}
