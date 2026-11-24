#pragma once

struct PidResult
{
    double error{};
    double proportional{};
    double integral{};
    double derivative{};
    double rawOutput{};
    double output{};
    bool saturated{};
};

class PidController
{
public:
    struct Gains
    {
        double kp{};
        double ki{};
        double kd{};
    };

    struct Limits
    {
        double minimum{0.0};
        double maximum{1.0};
    };

    explicit PidController(Gains gains);
    PidController(Gains gains, Limits limits, bool antiWindup = true);

    PidResult update(double setpoint, double measurement, double deltaTime);
    void reset() noexcept;

private:
    Gains gains_;
    Limits limits_;
    bool antiWindup_;
    double integral_{0.0};
    double previousError_{0.0};
    bool hasPreviousError_{false};
};
