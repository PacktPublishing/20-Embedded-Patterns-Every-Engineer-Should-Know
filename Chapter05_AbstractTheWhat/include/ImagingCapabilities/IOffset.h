#pragma once

#include "Range.h"

namespace ImagingCapabilities
{

class IOffset
{
public:
    virtual ~IOffset() = default;

    virtual void setOffset(int value) = 0;
    virtual int offset() const = 0;
    virtual Range<int> offsetRange() const = 0;
    virtual bool isOffsetValid(int value) const = 0;
};

}
