#pragma once

#include "PidController.h"
#include "SmokerPlant.h"

#include <iosfwd>
#include <string_view>

enum class Scenario
{
    OpenLoop,
    P,
    PI,
    PID,
    Windup,
    Disturbance
};

[[nodiscard]] Scenario parseScenario(std::string_view name);
[[nodiscard]] std::string_view scenarioName(Scenario scenario) noexcept;

class Simulation
{
public:
    explicit Simulation(Scenario scenario);
    void run(std::ostream& output);

private:
    Scenario scenario_;
    SmokerPlant plant_;
    PidController controller_;
};
