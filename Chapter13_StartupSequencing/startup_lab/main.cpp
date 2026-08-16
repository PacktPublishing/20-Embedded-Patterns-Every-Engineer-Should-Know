// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "BoundedQueue.h"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace
{
enum class ComponentId : std::size_t
{
    configuration,
    sensor,
    calibration,
    processing,
    telemetry,
    count
};

enum class StartupState
{
    ready,
    failed
};

struct StartupEvent
{
    ComponentId component{};
    StartupState state{};
};

enum class ComponentStatus
{
    waiting,
    starting,
    ready,
    failed,
    blocked
};

struct ComponentSpec
{
    ComponentId id{};
    std::chrono::milliseconds startupDelay{};
    bool required{true};
};

constexpr std::size_t ComponentCount =
    static_cast<std::size_t>(ComponentId::count);

constexpr std::array<ComponentSpec, ComponentCount> Components{{
    {ComponentId::configuration, 100ms, true},
    {ComponentId::sensor,        350ms, true},
    {ComponentId::calibration,   450ms, true},
    {ComponentId::processing,    200ms, true},
    {ComponentId::telemetry,     250ms, false},
}};

constexpr std::chrono::milliseconds DefaultFailureTimeout{2500};
constexpr std::size_t StartupQueueCapacity = 8;

std::size_t indexOf(ComponentId id)
{
    return static_cast<std::size_t>(id);
}

std::string_view componentName(ComponentId id)
{
    switch (id)
    {
    case ComponentId::configuration: return "configuration";
    case ComponentId::sensor:        return "sensor";
    case ComponentId::calibration:   return "calibration";
    case ComponentId::processing:    return "processing";
    case ComponentId::telemetry:     return "telemetry";
    case ComponentId::count:         break;
    }

    return "unknown";
}

std::optional<ComponentId> parseComponent(std::string_view name)
{
    for (const auto& component : Components)
    {
        if (componentName(component.id) == name)
        {
            return component.id;
        }
    }

    return std::nullopt;
}

std::optional<std::chrono::milliseconds> parseMilliseconds(std::string_view text)
{
    long long value{};
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);

    if (result.ec != std::errc{} || result.ptr != end || value <= 0)
    {
        return std::nullopt;
    }

    return std::chrono::milliseconds{value};
}

void printUsage(std::string_view program)
{
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Options:\n"
        << "  --fail_component <id>    Make a component report FAILED. Repeatable.\n"
        << "  --failure_timeout <ms>   Maximum time allowed for application startup.\n"
        << "  --help                   Show this help.\n\n"
        << "Component ids:\n"
        << "  configuration sensor calibration processing telemetry\n";
}

const ComponentSpec& specFor(ComponentId id)
{
    return Components[indexOf(id)];
}

} // namespace

int main(int argc, char* argv[])
{
    std::set<ComponentId> failedComponents;
    auto failureTimeout = DefaultFailureTimeout;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument{argv[i]};

        if (argument == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }

        if (argument == "--fail_component")
        {
            if (++i >= argc)
            {
                std::cerr << "error: --fail_component requires a component id\n";
                return 2;
            }

            const std::string_view name{argv[i]};
            const auto component = parseComponent(name);
            if (!component)
            {
                std::cerr << "error: unknown component '" << name << "'\n";
                return 2;
            }

            failedComponents.insert(*component);
            continue;
        }

        if (argument == "--failure_timeout")
        {
            if (++i >= argc)
            {
                std::cerr << "error: --failure_timeout requires milliseconds\n";
                return 2;
            }

            const auto timeout = parseMilliseconds(argv[i]);
            if (!timeout)
            {
                std::cerr << "error: invalid --failure_timeout value '"
                          << argv[i] << "'\n";
                return 2;
            }

            failureTimeout = *timeout;
            continue;
        }

        std::cerr << "error: unknown option '" << argument << "'\n";
        printUsage(argv[0]);
        return 2;
    }

    BoundedQueue<StartupEvent, StartupQueueCapacity> startupEvents;
    std::array<ComponentStatus, ComponentCount> status{};
    status.fill(ComponentStatus::waiting);

    std::vector<std::jthread> workers;
    workers.reserve(ComponentCount);

    const auto startedAt = std::chrono::steady_clock::now();

    const auto elapsedMilliseconds = [&]()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - startedAt)
            .count();
    };

    const auto log = [&](ComponentId id, std::string_view message)
    {
        std::cout << '[' << elapsedMilliseconds() << " ms] "
                  << componentName(id) << ": " << message << '\n';
    };

    const auto startComponent = [&](ComponentId id)
    {
        const auto& spec = specFor(id);
        status[indexOf(id)] = ComponentStatus::starting;
        log(id, "starting");

        const bool shouldFail = failedComponents.contains(id);
        workers.emplace_back([&, spec, shouldFail]
        {
            std::this_thread::sleep_for(spec.startupDelay);
            const StartupState result = shouldFail
                ? StartupState::failed
                : StartupState::ready;
            startupEvents.push({spec.id, result});
        });
    };

    std::mutex timeoutMutex;
    std::condition_variable timeoutCondition;
    bool startupFinished = false;
    std::atomic<bool> timedOut{false};

    std::jthread timeoutThread([&]
    {
        std::unique_lock<std::mutex> lock(timeoutMutex);
        const bool finished = timeoutCondition.wait_for(
            lock,
            failureTimeout,
            [&] { return startupFinished; });

        if (!finished)
        {
            timedOut.store(true);
            startupEvents.stop();
        }
    });

    bool applicationFailed = false;
    bool degraded = false;

    const auto allRequiredReady = [&]
    {
        for (const auto& component : Components)
        {
            if (component.required &&
                status[indexOf(component.id)] != ComponentStatus::ready)
            {
                return false;
            }
        }
        return true;
    };

    const auto telemetryFinished = [&]
    {
        const auto telemetryStatus = status[indexOf(ComponentId::telemetry)];
        return telemetryStatus == ComponentStatus::ready ||
               telemetryStatus == ComponentStatus::failed ||
               telemetryStatus == ComponentStatus::blocked;
    };

    const auto blockWaitingComponents = [&]
    {
        for (const auto& component : Components)
        {
            auto& componentStatus = status[indexOf(component.id)];
            if (componentStatus == ComponentStatus::waiting)
            {
                componentStatus = ComponentStatus::blocked;
                log(component.id, "BLOCKED - dependency not ready");
            }
        }
    };

    startComponent(ComponentId::configuration);

    while (!applicationFailed && !(allRequiredReady() && telemetryFinished()))
    {
        const auto event = startupEvents.pop();

        if (timedOut.load())
        {
            break;
        }

        if (!event)
        {
            break;
        }

        auto& componentStatus = status[indexOf(event->component)];

        if (event->state == StartupState::failed)
        {
            componentStatus = ComponentStatus::failed;
            log(event->component, "FAILED");

            if (specFor(event->component).required)
            {
                applicationFailed = true;
                blockWaitingComponents();
                startupEvents.stop();
            }
            else
            {
                degraded = true;
            }

            continue;
        }

        componentStatus = ComponentStatus::ready;
        log(event->component, "READY");

        switch (event->component)
        {
        case ComponentId::configuration:
            startComponent(ComponentId::sensor);
            startComponent(ComponentId::telemetry);
            break;

        case ComponentId::sensor:
            startComponent(ComponentId::calibration);
            break;

        case ComponentId::calibration:
            startComponent(ComponentId::processing);
            break;

        case ComponentId::processing:
        case ComponentId::telemetry:
        case ComponentId::count:
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(timeoutMutex);
        startupFinished = true;
    }
    timeoutCondition.notify_one();
    startupEvents.stop();

    if (timedOut.load())
    {
        std::cout << "\nAPPLICATION STARTUP TIMED OUT after "
                  << failureTimeout.count() << " ms\n";

        for (const auto& component : Components)
        {
            const auto componentStatus = status[indexOf(component.id)];
            if (component.required && componentStatus != ComponentStatus::ready)
            {
                std::cout << "  " << componentName(component.id)
                          << ": NOT READY\n";
            }
        }
        return 1;
    }

    if (applicationFailed)
    {
        std::cout << "\nAPPLICATION FAILED after "
                  << elapsedMilliseconds() << " ms\n";
        return 1;
    }

    std::cout << "\nAPPLICATION READY";
    if (degraded)
    {
        std::cout << " (DEGRADED)";
    }
    std::cout << " after " << elapsedMilliseconds() << " ms\n";

    return 0;
}
