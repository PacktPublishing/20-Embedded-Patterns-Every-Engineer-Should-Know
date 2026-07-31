#include "RampDriver.h"
#include "Ramper.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>

namespace
{
void ramperMovesUpAndClamps()
{
    auto ramper = Ramper::create(20.0, 32.0, 5.0);
    assert(ramper);
    assert(ramper->stepsRemaining() == 3);

    assert(!ramper->step());
    assert(ramper->value() == 25.0);
    assert(!ramper->step());
    assert(ramper->value() == 30.0);
    assert(ramper->step());
    assert(ramper->value() == 32.0);
}

void ramperMovesDownAndClamps()
{
    auto ramper = Ramper::create(32.0, 18.0, 5.0);
    assert(ramper);

    assert(!ramper->step());
    assert(ramper->value() == 27.0);
    assert(!ramper->step());
    assert(ramper->value() == 22.0);
    assert(ramper->step());
    assert(ramper->value() == 18.0);
}

void ramperRejectsInvalidConfiguration()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    assert(!Ramper::create(nan, 10.0, 1.0));
    assert(!Ramper::create(0.0, nan, 1.0));
    assert(!Ramper::create(0.0, 10.0, 0.0));
    assert(!Ramper::create(0.0, 10.0, -1.0));
}

void fieldCompletesOnPredictedCycle()
{
    using namespace std::chrono_literals;

    const RampDriver::Commands starts{0.0, 5.0, 10.0, 20.0,
                                      50.0, 30.0, 10.0, 0.0};
    const RampDriver::Commands targets{10.0, 5.0, 25.0, 0.0,
                                       45.0, 60.0, 0.0, 5.0};

    auto driver = RampDriver::create(starts, targets, 5.0, 10ms);
    assert(driver);
    assert(driver->predictedCycles() == 6);
    assert(driver->activeRampers() == 7);

    RampDriver::CycleResult result{};
    do
    {
        result = driver->step();
    }
    while (!result.complete);

    assert(driver->cycle() == driver->predictedCycles());
    assert(driver->commands() == targets);
    assert(driver->activeRampers() == 0);
}
}

int main()
{
    ramperMovesUpAndClamps();
    ramperMovesDownAndClamps();
    ramperRejectsInvalidConfiguration();
    fieldCompletesOnPredictedCycle();
}

