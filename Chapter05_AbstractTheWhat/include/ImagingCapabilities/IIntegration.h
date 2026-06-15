#pragma once

#include <vector>

#include "Range.h"

namespace ImagingCapabilities
{

enum class IntegrationMode
{
    FreeRun,              // Programmable integration, internal trigger.
    ProgrammableHardware, // Programmable integration, hardware trigger.
    ProgrammableSoftware, // Programmable integration, software trigger.
    ExternalHardware,     // External integration, hardware trigger.
    ExternalSoftware      // External integration, software trigger.
};

using IntegrationModeList = std::vector<IntegrationMode>;

class IIntegrationModeControl
{
public:
    virtual ~IIntegrationModeControl() = default;

    virtual void setIntegrationMode(IntegrationMode mode) = 0;
    virtual IntegrationMode integrationMode() const = 0;
    virtual IntegrationModeList validIntegrationModes() const = 0;
    virtual bool isIntegrationModeValid(IntegrationMode mode) const = 0;
};

// For devices whose integration time is represented by a continuous range.
class IIntegrationTimeRange
{
public:
    virtual ~IIntegrationTimeRange() = default;

    virtual void setIntegrationTimeUs(int microseconds) = 0;
    virtual int integrationTimeUs() const = 0;
    virtual Range<int> integrationTimeRangeUs() const = 0;
    virtual bool isIntegrationTimeValidUs(int microseconds) const = 0;
};

// For devices whose integration time is represented by distinct supported values.
class IIntegrationTimeDiscrete
{
public:
    virtual ~IIntegrationTimeDiscrete() = default;

    virtual void setIntegrationTimeUs(int microseconds) = 0;
    virtual int integrationTimeUs() const = 0;
    virtual std::vector<int> validIntegrationTimesUs() const = 0;
    virtual bool isIntegrationTimeValidUs(int microseconds) const = 0;
};

}
