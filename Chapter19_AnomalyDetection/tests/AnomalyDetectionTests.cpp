// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include "AnomalyDetection.h"

#include <cassert>
#include <chrono>
#include <vector>

namespace
{

using Measurement = chapter19::QualifiedMeasurement<double>;

class Collector
{
public:
    void process(const Measurement& measurement)
    {
        received.push_back(measurement);
    }

    std::vector<Measurement> received;
};

} // namespace

int main()
{
    using namespace std::chrono_literals;
    using chapter19::MeasurementQuality;

    chapter19::RangeFilter<double> range{0.0, 100.0};
    assert(range.inRange(0.0));
    assert(range.inRange(100.0));
    assert(!range.inRange(100.1));

    chapter19::RateOfChangeChecker<double> rate{2.0};
    const auto start = std::chrono::steady_clock::time_point{};
    assert(rate.changeNormal(10.0, start));
    assert(rate.changeNormal(11.0, start + 1s));
    assert(!rate.changeNormal(20.0, start + 2s));

    chapter19::StuckSensorChecker<double> stuck{3};
    assert(stuck.valueNormal(5.0));
    assert(stuck.valueNormal(5.0));
    assert(!stuck.valueNormal(5.0));
    assert(stuck.valueNormal(5.1));

    chapter19::StaleSensorChecker stale{2s};
    assert(!stale.stale(start + 10s));
    stale.measurementReceived(start);
    assert(!stale.stale(start + 2s));
    assert(stale.stale(start + 3s));

    Collector collector;
    chapter19::AnomalyDetectionNode<double, Collector> node{
        0.0,
        100.0,
        2.0,
        3,
        2s,
        collector};

    node.process({10.0, MeasurementQuality::Good}, start);
    node.process({150.0, MeasurementQuality::Good}, start + 1s);
    node.process({11.0, MeasurementQuality::Good}, start + 2s);
    node.checkForStaleMeasurement(start + 5s);
    node.process({12.0, MeasurementQuality::Invalid}, start + 6s);

    assert(collector.received.size() == 5);
    assert(collector.received[0].quality == MeasurementQuality::Good);
    assert(collector.received[1].quality == MeasurementQuality::Invalid);
    assert(collector.received[2].quality == MeasurementQuality::Good);
    assert(collector.received[3].quality == MeasurementQuality::Suspect);
    assert(collector.received[4].quality == MeasurementQuality::Invalid);

    return 0;
}
