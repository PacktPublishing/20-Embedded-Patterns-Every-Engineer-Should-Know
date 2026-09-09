// SPDX-License-Identifier: MIT

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "ParameterRegistry.h"

namespace
{
using namespace ch16;

bool nearlyEqual(float lhs, float rhs, float epsilon = 0.0001F)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

void testDefaults()
{
    ParameterStore store;

    ReportingInterval::ValueType interval{};
    TemperatureLimit::ValueType limit{};
    OperatingModeParameter::ValueType mode{};

    assert(store.get<ReportingInterval>(interval));
    assert(store.get<TemperatureLimit>(limit));
    assert(store.get<OperatingModeParameter>(mode));

    assert(interval == ReportingInterval::defaultValue);
    assert(nearlyEqual(limit, TemperatureLimit::defaultValue));
    assert(mode == OperatingModeParameter::defaultValue);
    assert(store.source(ParameterID::ReportingInterval) == ParameterSource::CompiledDefault);
    assert(store.generation() == 0);
}

void testPrecedenceAndTransactionality()
{
    ParameterStore store;
    ParameterChangeEvent event{};

    assert(store.set<ReportingInterval>(
               1500,
               ParameterSource::PersistentStorage,
               &event) == UpdateResult::OK);
    assert(event.generation == 1);

    assert(store.set<ReportingInterval>(
               3000,
               ParameterSource::CommandLine,
               &event) == UpdateResult::OK);
    assert(event.generation == 2);

    const auto generationBeforeReject = store.generation();
    assert(store.set<ReportingInterval>(
               2000,
               ParameterSource::ConfigurationFile,
               &event) == UpdateResult::LOWER_PRECEDENCE);
    assert(store.generation() == generationBeforeReject);

    ReportingInterval::ValueType interval{};
    assert(store.get<ReportingInterval>(interval));
    assert(interval == 3000);
    assert(store.source(ParameterID::ReportingInterval) == ParameterSource::CommandLine);

    assert(store.set<ReportingInterval>(
               50,
               ParameterSource::Runtime,
               &event) == UpdateResult::INVALID_VALUE);
    assert(store.generation() == generationBeforeReject);
    assert(store.get<ReportingInterval>(interval));
    assert(interval == 3000);
}

void testRuntimePolicy()
{
    ParameterStore store;
    ParameterChangeEvent event{};

    assert(store.set<TemperatureLimit>(
               30.0F,
               ParameterSource::Runtime,
               &event) == UpdateResult::NOT_RUNTIME_WRITABLE);

    TemperatureLimit::ValueType limit{};
    assert(store.get<TemperatureLimit>(limit));
    assert(nearlyEqual(limit, TemperatureLimit::defaultValue));
    assert(store.generation() == 0);
}

void testRuntimeHandlerPath()
{
    ParameterStore store;
    ParameterChangeEvent event{};

    assert(store.set(
               ParameterID::ReportingInterval,
               "5000",
               ParameterSource::Runtime,
               &event) == UpdateResult::OK);
    assert(event.id == ParameterID::ReportingInterval);
    assert(event.source == ParameterSource::Runtime);
    assert(event.generation == 1);

    ReportingInterval::ValueType interval{};
    assert(store.get<ReportingInterval>(interval));
    assert(interval == 5000);

    assert(store.set(
               ParameterID::ReportingInterval,
               "not-a-number",
               ParameterSource::Runtime,
               &event) == UpdateResult::INVALID_VALUE);
    assert(store.generation() == 1);

    assert(store.set(
               ParameterID::OperatingMode,
               "service",
               ParameterSource::Runtime,
               &event) == UpdateResult::OK);

    OperatingMode mode{};
    assert(store.get<OperatingModeParameter>(mode));
    assert(mode == OperatingMode::Service);
}

void testFixedRegistryAndImage()
{
    static_assert(parameterCount == 3);
    static_assert(ParameterImageSize == 9);
    static_assert(handlerFor(ParameterID::ReportingInterval).id ==
                  ParameterID::ReportingInterval);

    assert(parameterIdFromName("reporting_interval") ==
           ParameterID::ReportingInterval);
    assert(!parameterIdFromName("unknown"));

    ParameterStore store;
    assert(store.set<ReportingInterval>(
               5000,
               ParameterSource::Runtime) == UpdateResult::OK);

    const auto image = store.image();
    assert(image.size() == ParameterImageSize);

    // 5000 decimal is 0x00001388. The parameter image is little-endian.
    assert(std::to_integer<std::uint8_t>(image[0]) == 0x88u);
    assert(std::to_integer<std::uint8_t>(image[1]) == 0x13u);
    assert(std::to_integer<std::uint8_t>(image[2]) == 0x00u);
    assert(std::to_integer<std::uint8_t>(image[3]) == 0x00u);
}

} // namespace

int main()
{
    testDefaults();
    testPrecedenceAndTransactionality();
    testRuntimePolicy();
    testRuntimeHandlerPath();
    testFixedRegistryAndImage();

    std::cout << "All Chapter 16 parameter-management tests passed.\n";
    return 0;
}
