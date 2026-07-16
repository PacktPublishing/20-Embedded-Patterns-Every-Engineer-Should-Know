// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;
using Microseconds = std::chrono::microseconds;

volatile std::uint64_t busy_sink = 0;

struct Options
{
    std::chrono::microseconds period {10'000};
    int samples {2'000};
    std::vector<std::chrono::microseconds> stages {std::chrono::microseconds {500}};
    std::string outputPath {"latency.csv"};
};

void printUsage(const char* program)
{
    std::cerr
        << "Usage:\n"
        << "  " << program << " [options]\n\n"
        << "Options:\n"
        << "  --period-ms N     Period between samples in milliseconds. Default: 10\n"
        << "  --period-us N     Period between samples in microseconds.\n"
        << "  --samples N       Number of samples to collect. Default: 2000\n"
        << "  --work-us N       Single simulated pipeline stage duration. Default: 500\n"
        << "  --stage-us N      Add one simulated pipeline stage. May be repeated.\n"
        << "  --output PATH     CSV output path. Default: latency.csv\n"
        << "  --help            Show this help.\n\n"
        << "Examples:\n"
        << "  " << program << " --period-ms 10 --samples 2000 --work-us 500 --output quiet.csv\n"
        << "  " << program << " --period-ms 10 --samples 2000 --stage-us 300 --stage-us 400 --output pipeline.csv\n";
}

int parseInt(std::string_view text, std::string_view name)
{
    try
    {
        std::string value {text};
        std::size_t parsed = 0;
        int result = std::stoi(value, &parsed);

        if (parsed != value.size() || result <= 0)
        {
            throw std::invalid_argument("invalid");
        }

        return result;
    }
    catch (const std::exception&)
    {
        throw std::runtime_error("Invalid value for " + std::string(name) + ": " + std::string(text));
    }
}

Options parseOptions(int argc, char* argv[])
{
    Options options;
    bool explicitStages = false;

    for (int index = 1; index < argc; ++index)
    {
        std::string_view arg {argv[index]};

        auto requireValue = [&](std::string_view option) -> std::string_view {
            if (index + 1 >= argc)
            {
                throw std::runtime_error("Missing value for " + std::string(option));
            }

            ++index;
            return std::string_view {argv[index]};
        };

        if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            std::exit(EXIT_SUCCESS);
        }
        else if (arg == "--period-ms")
        {
            int value = parseInt(requireValue(arg), arg);
            options.period = std::chrono::milliseconds {value};
        }
        else if (arg == "--period-us")
        {
            int value = parseInt(requireValue(arg), arg);
            options.period = std::chrono::microseconds {value};
        }
        else if (arg == "--samples")
        {
            options.samples = parseInt(requireValue(arg), arg);
        }
        else if (arg == "--work-us")
        {
            int value = parseInt(requireValue(arg), arg);
            options.stages = {std::chrono::microseconds {value}};
            explicitStages = true;
        }
        else if (arg == "--stage-us")
        {
            int value = parseInt(requireValue(arg), arg);

            if (!explicitStages)
            {
                options.stages.clear();
                explicitStages = true;
            }

            options.stages.push_back(std::chrono::microseconds {value});
        }
        else if (arg == "--output")
        {
            options.outputPath = std::string {requireValue(arg)};
        }
        else
        {
            throw std::runtime_error("Unknown option: " + std::string(arg));
        }
    }

    if (options.stages.empty())
    {
        throw std::runtime_error("At least one pipeline stage is required");
    }

    return options;
}

std::int64_t toNanoseconds(Clock::duration duration)
{
    return std::chrono::duration_cast<Nanoseconds>(duration).count();
}

std::int64_t toMicroseconds(Clock::duration duration)
{
    return std::chrono::duration_cast<Microseconds>(duration).count();
}

void doBusyWorkFor(std::chrono::microseconds duration)
{
    const auto endTime = Clock::now() + duration;

    std::uint64_t value = 0x1234'5678u;

    while (Clock::now() < endTime)
    {
        value = value * 1'664'525u + 1'013'904'223u;
        value ^= value >> 13u;
    }

    busy_sink ^= value;
}

void runPipeline(const std::vector<std::chrono::microseconds>& stages)
{
    for (const auto stageDuration : stages)
    {
        doBusyWorkFor(stageDuration);
    }
}

double percentile(std::vector<std::int64_t> values, double p)
{
    if (values.empty())
    {
        return 0.0;
    }

    std::sort(values.begin(), values.end());

    const double index = p * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(index);
    const auto upper = std::min(lower + 1, values.size() - 1);
    const double fraction = index - static_cast<double>(lower);

    return static_cast<double>(values[lower]) * (1.0 - fraction)
        + static_cast<double>(values[upper]) * fraction;
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        const Options options = parseOptions(argc, argv);

        std::ofstream output {options.outputPath};

        if (!output)
        {
            std::cerr << "Failed to open output file: " << options.outputPath << '\n';
            return EXIT_FAILURE;
        }

        output
            << "sample_index,"
            << "scheduled_ns,"
            << "pipeline_begin_ns,"
            << "pipeline_end_ns,"
            << "wake_latency_us,"
            << "pipeline_duration_us,"
            << "total_latency_us,"
            << "overrun\n";

        std::vector<std::int64_t> totalLatenciesUs;
        totalLatenciesUs.reserve(static_cast<std::size_t>(options.samples));

        int overrunCount = 0;

        const auto startTime = Clock::now();

        for (int sample = 0; sample < options.samples; ++sample)
        {
            const auto scheduledTime = startTime + options.period * sample;

            std::this_thread::sleep_until(scheduledTime);

            const auto pipelineBeginTime = Clock::now();

            runPipeline(options.stages);

            const auto pipelineEndTime = Clock::now();

            const auto wakeLatency = pipelineBeginTime - scheduledTime;
            const auto pipelineDuration = pipelineEndTime - pipelineBeginTime;
            const auto totalLatency = pipelineEndTime - scheduledTime;

            const bool overrun = totalLatency > options.period;

            if (overrun)
            {
                ++overrunCount;
            }

            const auto totalLatencyUs = toMicroseconds(totalLatency);
            totalLatenciesUs.push_back(totalLatencyUs);

            output
                << sample << ','
                << toNanoseconds(scheduledTime - startTime) << ','
                << toNanoseconds(pipelineBeginTime - startTime) << ','
                << toNanoseconds(pipelineEndTime - startTime) << ','
                << toMicroseconds(wakeLatency) << ','
                << toMicroseconds(pipelineDuration) << ','
                << totalLatencyUs << ','
                << (overrun ? 1 : 0) << '\n';
        }

        const auto minmax = std::minmax_element(totalLatenciesUs.begin(), totalLatenciesUs.end());
        const auto sum = std::accumulate(totalLatenciesUs.begin(), totalLatenciesUs.end(), std::int64_t {0});
        const double average = static_cast<double>(sum) / static_cast<double>(totalLatenciesUs.size());

        std::cerr << "Wrote " << options.outputPath << '\n';
        std::cerr << "Samples:  " << options.samples << '\n';
        std::cerr << "Period:   " << options.period.count() << " us\n";
        std::cerr << "Min:      " << *minmax.first << " us\n";
        std::cerr << "Average:  " << average << " us\n";
        std::cerr << "P95:      " << percentile(totalLatenciesUs, 0.95) << " us\n";
        std::cerr << "P99:      " << percentile(totalLatenciesUs, 0.99) << " us\n";
        std::cerr << "Max:      " << *minmax.second << " us\n";
        std::cerr << "Overruns: " << overrunCount << '\n';

        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        std::cerr << '\n';
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
}
