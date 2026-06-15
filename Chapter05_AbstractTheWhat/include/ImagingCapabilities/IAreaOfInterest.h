#pragma once

#include "Geometry.h"

namespace ImagingCapabilities
{

class IAreaOfInterest
{
public:
    virtual ~IAreaOfInterest() = default;

    virtual void setAreaOfInterest(const Rect& area) = 0;
    virtual Rect areaOfInterest() const = 0;
    virtual Rect maximumAreaOfInterest() const = 0;
    virtual bool isAreaOfInterestValid(const Rect& area) const = 0;
};

}
