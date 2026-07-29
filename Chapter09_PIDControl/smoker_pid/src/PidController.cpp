#include "PidController.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

PidController::PidController(Gains gains)
    : PidController{gains, Limits{}, true}
{
}

PidController::PidController(Gains gains, Limits limits, bool antiWindup)
    : gains_{gains}, limits_{limits}, antiWindup_{antiWindup}
{
    if (!std::isfinite(gains_.kp) || !std::isfinite(gains_.ki) ||
        !std::isfinite(gains_.kd) || gains_.kp < 0.0 || gains_.ki < 0.0 ||
        gains_.kd < 0.0 || !std::isfinite(limits_.minimum) ||
        !std::isfinite(limits_.maximum) || limits_.minimum >= limits_.maximum)
    {
        throw std::invalid_argument{"invalid PID gains or output limits"};
    }
}

PidResult PidController::update(double setpoint, double measurement, double deltaTime)
{
    if (!std::isfinite(setpoint) || !std::isfinite(measurement) ||
        !std::isfinite(deltaTime) || deltaTime <= 0.0)
    {
        throw std::invalid_argument{"PID inputs must be finite and delta time must be positive"};
    }

    PidResult result;
    result.error = setpoint - measurement;
    result.proportional = gains_.kp * result.error;
    result.derivative = hasPreviousError_
        ? gains_.kd * ((result.error - previousError_) / deltaTime)
        : 0.0;

    const double candidateIntegral = integral_ + gains_.ki * result.error * deltaTime;
    const double candidateOutput =
        result.proportional + candidateIntegral + result.derivative;

    const bool drivesHighSaturation =
        candidateOutput > limits_.maximum && result.error > 0.0;
    const bool drivesLowSaturation =
        candidateOutput < limits_.minimum && result.error < 0.0;

    if (!antiWindup_ || (!drivesHighSaturation && !drivesLowSaturation))
    {
        integral_ = candidateIntegral;
    }

    result.integral = integral_;
    result.rawOutput = result.proportional + result.integral + result.derivative;
    result.output = std::clamp(result.rawOutput, limits_.minimum, limits_.maximum);
    result.saturated = result.output != result.rawOutput;

    previousError_ = result.error;
    hasPreviousError_ = true;
    return result;
}

void PidController::reset() noexcept
{
    integral_ = 0.0;
    previousError_ = 0.0;
    hasPreviousError_ = false;
}
