// SPDX-License-Identifier: MIT

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <syncstream>
#include <thread>

#include "BoundedQueue.h"
#include "ParameterMessages.h"
#include "ParameterRegistry.h"

namespace
{
using namespace ch16;
using namespace std::chrono_literals;

void printStore(const ParameterStore& store)
{
    ReportingInterval::ValueType interval{};
    TemperatureLimit::ValueType limit{};
    OperatingModeParameter::ValueType mode{};

    const bool ok =
        store.get<ReportingInterval>(interval) &&
        store.get<TemperatureLimit>(limit) &&
        store.get<OperatingModeParameter>(mode);

    if (!ok)
    {
        std::osyncstream(std::cout) << "Could not decode parameter image\n";
        return;
    }

    std::osyncstream out(std::cout);
    out << "Current parameter state\n"
        << "  reporting_interval = " << interval << " ms"
        << "  [" << sourceName(store.source(ParameterID::ReportingInterval)) << "]\n"
        << "  temperature_limit  = " << limit << " C"
        << "  [" << sourceName(store.source(ParameterID::TemperatureLimit)) << "]\n"
        << "  operating_mode     = " << operatingModeName(mode)
        << "  [" << sourceName(store.source(ParameterID::OperatingMode)) << "]\n"
        << "  generation         = " << store.generation() << "\n";
}

void printEvent(const ParameterChangeEvent& event, const ParameterStore& store)
{
    std::osyncstream out(std::cout);
    out << "NOTIFY generation=" << event.generation
        << " parameter=" << handlerFor(event.id).name
        << " source=" << sourceName(event.source);

    switch (event.id)
    {
    case ParameterID::ReportingInterval:
    {
        ReportingInterval::ValueType value{};
        if (store.get<ReportingInterval>(value))
        {
            out << " current=" << value << " ms";
        }
        break;
    }
    case ParameterID::TemperatureLimit:
    {
        TemperatureLimit::ValueType value{};
        if (store.get<TemperatureLimit>(value))
        {
            out << " current=" << value << " C";
        }
        break;
    }
    case ParameterID::OperatingMode:
    {
        OperatingModeParameter::ValueType value{};
        if (store.get<OperatingModeParameter>(value))
        {
            out << " current=" << operatingModeName(value);
        }
        break;
    }
    case ParameterID::Count:
        break;
    }
    out << '\n';
}

} // namespace

int main()
{
    using namespace ch16;

    ParameterStore parameterStore;
    BoundedQueue<ParameterUpdateRequest, 8> updateQueue;
    BoundedQueue<ParameterChangeEvent, 8> notificationQueue;

    std::cout << "Chapter 16 parameter-management lab\n"
              << "Fixed BDS image size: " << ParameterImageSize << " bytes\n\n";

    printStore(parameterStore);
    std::cout << '\n';

    // One producer models all incoming interface traffic for the lab. The
    // ParameterSource field preserves where each request originated.
    std::thread requestProducer([&updateQueue]()
    {
        struct ScriptedRequest
        {
            ParameterID id;
            std::string_view text;
            ParameterSource source;
        };

        constexpr ScriptedRequest requests[]{
            {ParameterID::ReportingInterval, "1500", ParameterSource::PersistentStorage},
            {ParameterID::ReportingInterval, "2000", ParameterSource::ConfigurationFile},
            {ParameterID::ReportingInterval, "3000", ParameterSource::CommandLine},
            {ParameterID::ReportingInterval, "2500", ParameterSource::ConfigurationFile},
            {ParameterID::ReportingInterval, "5000", ParameterSource::Runtime},
            {ParameterID::ReportingInterval, "50", ParameterSource::Runtime},
            {ParameterID::TemperatureLimit, "30", ParameterSource::Runtime},
            {ParameterID::OperatingMode, "service", ParameterSource::Runtime}
        };

        for (const auto& item : requests)
        {
            const auto request = makeUpdateRequest(item.id, item.text, item.source);
            if (!request)
            {
                std::osyncstream(std::cout)
                    << "Request too large for bounded message buffer\n";
                continue;
            }

            std::osyncstream(std::cout)
                << "REQUEST parameter=" << handlerFor(item.id).name
                << " value=" << item.text
                << " source=" << sourceName(item.source) << '\n';

            if (!updateQueue.push(*request))
            {
                break;
            }

            std::this_thread::sleep_for(25ms);
        }

        updateQueue.stop();
    });

    std::thread parameterOwner([&]()
    {
        while (const auto request = updateQueue.pop())
        {
            ParameterChangeEvent event{};
            const auto result = parameterStore.set(
                request->id,
                request->text(),
                request->source,
                &event);

            std::osyncstream(std::cout)
                << "RESULT  parameter=" << handlerFor(request->id).name
                << " value=" << request->text()
                << " -> " << resultName(result) << '\n';

            if (result == UpdateResult::OK)
            {
                // This queue has one producer (the parameter owner) and one
                // consumer (the reporting/notification thread).
                if (!notificationQueue.push(event))
                {
                    break;
                }
            }
        }

        notificationQueue.stop();
    });

    std::thread notificationConsumer([&]()
    {
        while (const auto event = notificationQueue.pop())
        {
            printEvent(*event, parameterStore);
        }
    });

    requestProducer.join();
    parameterOwner.join();
    notificationConsumer.join();

    std::cout << '\n';
    printStore(parameterStore);

    return 0;
}
