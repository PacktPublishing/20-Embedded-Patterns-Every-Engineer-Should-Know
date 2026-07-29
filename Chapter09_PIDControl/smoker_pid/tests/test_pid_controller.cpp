#include "PidController.h"

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

    PidController proportional{{0.1, 0.0, 0.0}, {-10.0, 10.0}};
    auto result = proportional.update(100.0, 95.0, 1.0);
    check(near(result.proportional, 0.5), "proportional term");
    check(near(result.derivative, 0.0), "first derivative is zero");
    check(near(result.output, 0.5), "proportional output");

    PidController complete{{0.1, 0.01, 0.2}, {-10.0, 10.0}};
    result = complete.update(100.0, 95.0, 1.0);
    check(near(result.integral, 0.05), "integral accumulates");
    result = complete.update(100.0, 97.0, 1.0);
    check(near(result.proportional, 0.3), "second proportional term");
    check(near(result.integral, 0.08), "second integral term");
    check(near(result.derivative, -0.4), "derivative responds to error change");
    check(near(result.output, -0.02), "combined PID output");

    PidController protectedController{{1.0, 1.0, 0.0}, {0.0, 1.0}, true};
    result = protectedController.update(10.0, 0.0, 1.0);
    check(result.saturated, "output saturates");
    check(near(result.integral, 0.0), "anti-windup blocks harmful integration");

    protectedController.reset();
    result = protectedController.update(0.5, 0.0, 1.0);
    check(near(result.derivative, 0.0), "reset clears derivative history");

    if (failures == 0)
    {
        std::cout << "PidController tests: PASS\n";
    }
    return failures == 0 ? 0 : 1;
}
