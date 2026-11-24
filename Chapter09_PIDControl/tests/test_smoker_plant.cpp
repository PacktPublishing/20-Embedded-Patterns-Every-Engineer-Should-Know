#include "SmokerPlant.h"

#include <cmath>
#include <iostream>

namespace
{
bool near(double actual, double expected, double tolerance = 1e-9)
{
    return std::abs(actual - expected) <= tolerance;
}
}

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };

    SmokerPlant plant{{75.0, 6.0, 0.025, 75.0}};
    check(near(plant.temperature(), 75.0), "starts at initial temperature");

    plant.update(1.0, 1.0);
    check(near(plant.temperature(), 81.0), "full fan adds heat");

    plant.update(0.0, 1.0);
    check(near(plant.temperature(), 80.85), "plant loses heat toward ambient");

    plant.applyTemperatureDrop(3.0);
    check(near(plant.temperature(), 77.85), "disturbance drops temperature");

    plant.reset();
    check(near(plant.temperature(), 75.0), "reset restores initial temperature");

    if (failures == 0)
    {
        std::cout << "SmokerPlant tests: PASS\n";
    }
    return failures == 0 ? 0 : 1;
}
