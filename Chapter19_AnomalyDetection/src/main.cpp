// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "AnomalyDetection.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace
{

using chapter19::MeasurementQuality;
using chapter19::QualifiedMeasurement;

std::string_view toString(MeasurementQuality quality)
{
    switch (quality)
    {
    case MeasurementQuality::Good:
        return "Good";
    case MeasurementQuality::Suspect:
        return "Suspect";
    case MeasurementQuality::Invalid:
        return "Invalid";
    }

    return "Unknown";
}

class TemperatureDisplayNode
{
public:
    void setScenario(std::string_view scenario)
    {
        scenario_ = scenario;
    }

    void process(const QualifiedMeasurement<double>& measurement)
    {
        std::cout << std::left << std::setw(24) << scenario_
                  << " value=" << std::fixed << std::setprecision(1)
                  << std::setw(6) << measurement.measurement
                  << " quality=" << toString(measurement.quality) << '\n';
    }

private:
    std::string_view scenario_{"measurement"};
};

} // namespace

int main()
{
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    TemperatureDisplayNode display;

    chapter19::AnomalyDetectionNode<double, TemperatureDisplayNode>
        anomalyNode{
            -40.0,
            85.0,
            5.0,
            4,
            3s,
            display};

    chapter19::MeasurementSourceNode<double, decltype(anomalyNode)>
        source{anomalyNode};

    const auto start = Clock::time_point{};

    display.setScenario("initial reading");
    source.emit(20.0, start);

    display.setScenario("normal change");
    source.emit(20.5, start + 1s);

    display.setScenario("out of range");
    source.emit(150.0, start + 2s);

    display.setScenario("normal recovery");
    source.emit(20.7, start + 3s);

    display.setScenario("rapid change");
    source.emit(35.0, start + 4s);

    display.setScenario("repeat 2");
    source.emit(35.0, start + 5s);

    display.setScenario("repeat 3");
    source.emit(35.0, start + 6s);

    display.setScenario("stuck reading");
    source.emit(35.0, start + 7s);

    display.setScenario("stuck recovery");
    source.emit(34.5, start + 8s);

    display.setScenario("stale reading");
    anomalyNode.checkForStaleMeasurement(start + 12s);

    display.setScenario("driver invalid");
    source.emit(
        34.5,
        start + 13s,
        MeasurementQuality::Invalid);

    return 0;
}
