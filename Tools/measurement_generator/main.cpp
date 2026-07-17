// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "MeasurementGenerator.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

struct ProgramOptions
{
    measurement::GeneratorConfig generator{};
    std::string output_path{"measurements.csv"};
};

[[nodiscard]] std::string_view require_value(int argc,
                                             char* argv[],
                                             int& index,
                                             std::string_view option)
{
    if(index + 1 >= argc)
    {
        throw std::invalid_argument(std::string{option} +
                                    " requires a value");
    }

    ++index;
    return argv[index];
}

template<typename Integer>
[[nodiscard]] Integer parse_integer(std::string_view text,
                                    std::string_view option)
{
    Integer value{};
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);

    if(error != std::errc{} || next != end)
    {
        throw std::invalid_argument(std::string{option} +
                                    " requires an integer value");
    }

    return value;
}

[[nodiscard]] double parse_double(std::string_view text,
                                  std::string_view option)
{
    std::string value_text{text};
    char* end = nullptr;
    const double value = std::strtod(value_text.c_str(), &end);

    if(end == value_text.c_str() || *end != '\0')
    {
        throw std::invalid_argument(std::string{option} +
                                    " requires a numeric value");
    }

    return value;
}

void print_usage(std::ostream& output, std::string_view program_name)
{
    output
        << "Usage: " << program_name << " [options]\n\n"
        << "Generate a deterministic physical signal with simulated "
           "measurement artifacts.\n\n"
        << "Options:\n"
        << "  --count N                 Number of measurement opportunities\n"
        << "  --minimum VALUE           Minimum underlying signal value\n"
        << "  --maximum VALUE           Maximum underlying signal value\n"
        << "  --profile NAME            constant, ramp, triangle, or step\n"
        << "  --sample-rate HZ          Measurement opportunities per second\n"
        << "  --noise-stddev VALUE      Gaussian noise standard deviation\n"
        << "  --spike-frequency VALUE   Spike probability from 0.0 to 1.0\n"
        << "  --missing-frequency VALUE Missing probability from 0.0 to 1.0\n"
        << "  --spike-minimum VALUE     Minimum absolute spike magnitude\n"
        << "  --spike-maximum VALUE     Maximum absolute spike magnitude\n"
        << "  --seed N                  Deterministic random seed\n"
        << "  --output PATH             Output CSV file\n"
        << "  --help                    Show this help text\n";
}

[[nodiscard]] ProgramOptions parse_options(int argc, char* argv[])
{
    ProgramOptions options{};

    for(int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if(argument == "--help")
        {
            print_usage(std::cout, argv[0]);
            std::exit(EXIT_SUCCESS);
        }
        if(argument == "--count")
        {
            options.generator.count = parse_integer<std::size_t>(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--minimum")
        {
            options.generator.minimum = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--maximum")
        {
            options.generator.maximum = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--profile")
        {
            const auto value = require_value(argc, argv, index, argument);
            const auto profile = measurement::parse_signal_profile(value);
            if(!profile)
            {
                throw std::invalid_argument(
                    "profile must be constant, ramp, triangle, or step");
            }
            options.generator.profile = *profile;
        }
        else if(argument == "--sample-rate")
        {
            options.generator.sample_rate_hz = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--noise-stddev")
        {
            options.generator.noise_stddev = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--spike-frequency")
        {
            options.generator.spike_frequency = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--missing-frequency")
        {
            options.generator.missing_frequency = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--spike-minimum")
        {
            options.generator.spike_minimum = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--spike-maximum")
        {
            options.generator.spike_maximum = parse_double(
                require_value(argc, argv, index, argument), argument);
        }
        else if(argument == "--seed")
        {
            const auto value = parse_integer<std::uint64_t>(
                require_value(argc, argv, index, argument), argument);
            if(value > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::invalid_argument(
                    "seed must fit in an unsigned 32-bit integer");
            }
            options.generator.seed = static_cast<std::uint32_t>(value);
        }
        else if(argument == "--output")
        {
            options.output_path =
                require_value(argc, argv, index, argument);
        }
        else
        {
            throw std::invalid_argument("unknown option: " +
                                        std::string{argument});
        }
    }

    measurement::validate(options.generator);
    return options;
}

void write_csv(const std::string& path,
               const std::vector<measurement::GeneratedMeasurement>& values)
{
    std::ofstream output{path};
    if(!output)
    {
        throw std::runtime_error("unable to open output file: " + path);
    }

    output << "index,time_s,true_value,measured_value,event\n";
    output << std::fixed << std::setprecision(6);

    for(const auto& value : values)
    {
        output << value.index << ',' << value.time_seconds << ','
               << value.true_value << ',';

        if(value.measured_value)
        {
            output << *value.measured_value;
        }

        output << ',' << measurement::to_string(value.event) << '\n';
    }

    if(!output)
    {
        throw std::runtime_error("failed while writing output file: " + path);
    }
}

void print_summary(
    const ProgramOptions& options,
    const std::vector<measurement::GeneratedMeasurement>& measurements)
{
    const auto summary = measurement::summarize(measurements);

    std::cout << "Measurement generation complete\n\n"
              << "Measurements requested: " << measurements.size() << '\n'
              << "Normal measurements:    " << summary.normal_count << '\n'
              << "Spikes generated:       " << summary.spike_count << '\n'
              << "Missing measurements:   " << summary.missing_count << '\n'
              << "Signal profile:         "
              << measurement::to_string(options.generator.profile) << '\n'
              << "Random seed:            " << options.generator.seed << '\n'
              << "Output:                 " << options.output_path << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        const auto options = parse_options(argc, argv);
        measurement::MeasurementGenerator generator{options.generator};
        const auto measurements = generator.generate();

        write_csv(options.output_path, measurements);
        print_summary(options, measurements);
        return EXIT_SUCCESS;
    }
    catch(const std::exception& error)
    {
        std::cerr << "error: " << error.what() << "\n\n";
        print_usage(std::cerr, argc > 0 ? argv[0] : "measurement_generator");
        return EXIT_FAILURE;
    }
}
