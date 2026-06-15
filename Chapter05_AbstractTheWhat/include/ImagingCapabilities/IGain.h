#pragma once

#include "Range.h"

namespace ImagingCapabilities
{

class IGain
{
public:
    virtual ~IGain() = default;

    virtual void setGain(int value) = 0;
    virtual int gain() const = 0;
    virtual Range<int> gainRange() const = 0;
    virtual bool isGainValid(int value) const = 0;
};

}
