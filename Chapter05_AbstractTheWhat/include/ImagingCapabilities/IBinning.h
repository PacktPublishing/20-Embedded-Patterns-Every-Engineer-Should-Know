#pragma once

#include <vector>

#include "Geometry.h"

namespace ImagingCapabilities
{

class IBinning
{
public:
    virtual ~IBinning() = default;

    virtual void setBinning(const Dimensions& binning) = 0;
    virtual Dimensions binning() const = 0;
    virtual std::vector<Dimensions> validBinningValues() const = 0;
    virtual bool isBinningValid(const Dimensions& binning) const = 0;
};

}
