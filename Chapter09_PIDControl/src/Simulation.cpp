#include "Simulation.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>

namespace
{
constexpr double setpoint = 250.0;
constexpr double deltaTime = 1.0;
constexpr int durationSeconds = 900;

PidController::Gains gainsFor(Scenario scenario)
{
    switch (scenario)
    {
    case Scenario::OpenLoop: return {};
    case Scenario::P: return {0.012, 0.0, 0.0};
    case Scenario::PI: return {0.012, 0.00022, 0.0};
    case Scenario::PID: return {0.012, 0.00022, 0.06};
    case Scenario::Windup: return {0.012, 0.0015, 0.0};
    case Scenario::Disturbance: return {0.012, 0.00022, 0.06};
    }
    throw std::logic_error{"unknown scenario"};
}
}

Scenario parseScenario(std::string_view name)
{
    if (name == "open-loop") return Scenario::OpenLoop;
    if (name == "p") return Scenario::P;
    if (name == "pi") return Scenario::PI;
    if (name == "pid") return Scenario::PID;
    if (name == "windup") return Scenario::Windup;
    if (name == "disturbance") return Scenario::Disturbance;
    throw std::invalid_argument{"unknown scenario: " + std::string{name}};
}

std::string_view scenarioName(Scenario scenario) noexcept
{
    switch (scenario)
    {
    case Scenario::OpenLoop: return "open-loop";
    case Scenario::P: return "p";
    case Scenario::PI: return "pi";
    case Scenario::PID: return "pid";
    case Scenario::Windup: return "windup";
    case Scenario::Disturbance: return "disturbance";
    }
    return "unknown";
}

Simulation::Simulation(Scenario scenario)
    : scenario_{scenario},
      plant_{SmokerPlant::Parameters{}},
      controller_{gainsFor(scenario), PidController::Limits{}, scenario != Scenario::Windup}
{
}

void Simulation::run(std::ostream& output)
{
    output << "time_s,setpoint_f,temperature_f,error_f,p_term,i_term,d_term,"
              "raw_output,fan_command,saturated,disturbance\n";
    output << std::fixed << std::setprecision(6);

    for (int second = 0; second <= durationSeconds; ++second)
    {
        bool disturbance = false;
        if (scenario_ == Scenario::Disturbance && second == 500)
        {
            plant_.applyTemperatureDrop(55.0);
            disturbance = true;
        }

        PidResult result;
        double fanCommand = 0.55;
        if (scenario_ != Scenario::OpenLoop)
        {
            result = controller_.update(setpoint, plant_.temperature(), deltaTime);
            fanCommand = result.output;
        }
        else
        {
            result.error = setpoint - plant_.temperature();
            result.rawOutput = fanCommand;
            result.output = fanCommand;
        }

        output << second << ',' << setpoint << ',' << plant_.temperature() << ','
               << result.error << ',' << result.proportional << ',' << result.integral
               << ',' << result.derivative << ',' << result.rawOutput << ','
               << fanCommand << ',' << (result.saturated ? 1 : 0) << ','
               << (disturbance ? 1 : 0) << '\n';

        if (second != durationSeconds)
        {
            plant_.update(fanCommand, deltaTime);
        }
    }
}
