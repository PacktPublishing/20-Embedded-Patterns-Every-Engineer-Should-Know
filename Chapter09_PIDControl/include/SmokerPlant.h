#pragma once

class SmokerPlant
{
public:
    struct Parameters
    {
        double ambientTemperature{75.0};
        double heatingCoefficient{6.0};
        double heatLossCoefficient{0.025};
        double initialTemperature{75.0};
    };

    explicit SmokerPlant(Parameters parameters);

    void update(double fanCommand, double deltaTime);
    void applyTemperatureDrop(double degrees);
    [[nodiscard]] double temperature() const noexcept;
    void reset() noexcept;

private:
    Parameters parameters_;
    double temperature_;
};
