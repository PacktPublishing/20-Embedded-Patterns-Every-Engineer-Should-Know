// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "ExponentialFilter.h"
#include "MovingAverage.h"

#include <MeasurementValidator.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{

struct Options
{
    std::string input_path;
    std::string output_path{"filtered_measurements.csv"};
    double minimum_value{0.0};
    double maximum_value{50.0};
    double maximum_rate_per_second{2.0};
    std::optional<std::size_t> moving_average_window;
    std::optional<double> exponential_alpha;
};

struct StatusCounts
{
    std::size_t valid{0};
    std::size_t missing{0};
    std::size_t non_finite{0};
    std::size_t out_of_range{0};
    std::size_t implausible_rate_of_change{0};

    void add(filtering::MeasurementStatus status)
    {
        switch(status)
        {
        case filtering::MeasurementStatus::Valid:
            ++valid;
            break;

        case filtering::MeasurementStatus::Missing:
            ++missing;
            break;

        case filtering::MeasurementStatus::NonFinite:
            ++non_finite;
            break;

        case filtering::MeasurementStatus::OutOfRange:
            ++out_of_range;
            break;

        case filtering::MeasurementStatus::ImplausibleRateOfChange:
            ++implausible_rate_of_change;
            break;
        }
    }

    [[nodiscard]] std::size_t total() const noexcept
    {
        return valid + missing + non_finite + out_of_range +
               implausible_rate_of_change;
    }
};

[[noreturn]] void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

void print_usage(std::ostream& output)
{
    output
        << "Usage: filter_measurements [options] INPUT.csv\n\n"
        << "Validate and filter measurements produced by "
           "measurement_generator.\n\n"
        << "Options:\n"
        << "  --output PATH                Output CSV file "
           "(default: filtered_measurements.csv)\n"
        << "  --minimum VALUE              Minimum valid measurement "
           "(default: 0.0)\n"
        << "  --maximum VALUE              Maximum valid measurement "
           "(default: 50.0)\n"
        << "  --maximum-rate VALUE         Maximum valid rate of change "
           "per second (default: 2.0)\n"
        << "  --moving-average-window N    Enable an N-sample moving average\n"
        << "  --exponential-alpha VALUE    Enable an exponential filter; "
           "0.0 < alpha <= 1.0\n"
        << "  --help                       Show this help text\n\n"
        << "At least one filter option is required. Both may be enabled so "
           "their outputs can be compared.\n";
}

[[nodiscard]] double parse_finite_double(
    std::string_view text,
    std::string_view description)
{
    double value{};
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);

    if(error != std::errc{} || position != end || !std::isfinite(value))
    {
        fail("invalid numeric value for " + std::string{description} +
             ": " + std::string{text});
    }

    return value;
}

[[nodiscard]] double parse_measurement_double(
    std::string_view text,
    std::size_t line_number)
{
    if(text == "nan" || text == "NaN" || text == "NAN")
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if(text == "inf" || text == "+inf" || text == "Inf" ||
       text == "+Inf" || text == "INF" || text == "+INF")
    {
        return std::numeric_limits<double>::infinity();
    }

    if(text == "-inf" || text == "-Inf" || text == "-INF")
    {
        return -std::numeric_limits<double>::infinity();
    }

    double value{};
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);

    if(error != std::errc{} || position != end)
    {
        fail("invalid measured_value on CSV line " +
             std::to_string(line_number) + ": " + std::string{text});
    }

    return value;
}

[[nodiscard]] std::size_t parse_size(
    std::string_view text,
    std::string_view description)
{
    std::size_t value{};
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);

    if(error != std::errc{} || position != end || value == 0)
    {
        fail("invalid positive integer for " + std::string{description} +
             ": " + std::string{text});
    }

    return value;
}

[[nodiscard]] Options parse_options(int argc, char* argv[])
{
    Options options;

    for(int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if(argument == "--help")
        {
            print_usage(std::cout);
            std::exit(0);
        }

        auto require_value = [&](std::string_view option) {
            if(index + 1 >= argc)
            {
                fail("missing value for " + std::string{option});
            }

            ++index;
            return std::string_view{argv[index]};
        };

        if(argument == "--output")
        {
            options.output_path = require_value(argument);
        }
        else if(argument == "--minimum")
        {
            options.minimum_value =
                parse_finite_double(require_value(argument), argument);
        }
        else if(argument == "--maximum")
        {
            options.maximum_value =
                parse_finite_double(require_value(argument), argument);
        }
        else if(argument == "--maximum-rate")
        {
            options.maximum_rate_per_second =
                parse_finite_double(require_value(argument), argument);
        }
        else if(argument == "--moving-average-window")
        {
            options.moving_average_window =
                parse_size(require_value(argument), argument);
        }
        else if(argument == "--exponential-alpha")
        {
            options.exponential_alpha =
                parse_finite_double(require_value(argument), argument);
        }
        else if(argument.starts_with("--"))
        {
            fail("unknown option: " + std::string{argument});
        }
        else if(options.input_path.empty())
        {
            options.input_path = argument;
        }
        else
        {
            fail("unexpected argument: " + std::string{argument});
        }
    }

    if(options.input_path.empty())
    {
        fail("an input CSV file is required");
    }

    if(!options.moving_average_window && !options.exponential_alpha)
    {
        fail("enable at least one filter with --moving-average-window "
             "or --exponential-alpha");
    }

    return options;
}

[[nodiscard]] std::vector<std::string_view> split_csv_line(
    const std::string& line)
{
    std::vector<std::string_view> fields;
    std::size_t begin = 0;

    while(true)
    {
        const std::size_t comma = line.find(',', begin);

        if(comma == std::string::npos)
        {
            fields.emplace_back(line.data() + begin, line.size() - begin);
            break;
        }

        fields.emplace_back(line.data() + begin, comma - begin);
        begin = comma + 1;
    }

    return fields;
}

[[nodiscard]] std::unordered_map<std::string, std::size_t>
make_column_map(const std::vector<std::string_view>& header_fields)
{
    std::unordered_map<std::string, std::size_t> columns;

    for(std::size_t index = 0; index < header_fields.size(); ++index)
    {
        columns.emplace(std::string{header_fields[index]}, index);
    }

    return columns;
}

[[nodiscard]] std::size_t require_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    std::string_view name)
{
    const auto iterator = columns.find(std::string{name});

    if(iterator == columns.end())
    {
        fail("input CSV is missing required column: " + std::string{name});
    }

    return iterator->second;
}

[[nodiscard]] std::optional<double> parse_optional_measurement(
    std::string_view text,
    std::size_t line_number)
{
    if(text.empty())
    {
        return std::nullopt;
    }

    return parse_measurement_double(text, line_number);
}

void write_optional_value(
    std::ostream& output,
    const std::optional<double>& value)
{
    if(value)
    {
        output << std::fixed << std::setprecision(6) << *value;
    }
}

void print_summary(
    const StatusCounts& counts,
    const Options& options)
{
    std::cout
        << "Processed " << counts.total()
        << " measurement opportunities\n"
        << "  Valid: " << counts.valid << '\n'
        << "  Missing: " << counts.missing << '\n'
        << "  Non-finite: " << counts.non_finite << '\n'
        << "  Out of range: " << counts.out_of_range << '\n'
        << "  Implausible rate of change: "
        << counts.implausible_rate_of_change << '\n';

    if(options.moving_average_window)
    {
        std::cout << "  Moving-average window: "
                  << *options.moving_average_window << '\n';
    }

    if(options.exponential_alpha)
    {
        std::cout << "  Exponential alpha: "
                  << *options.exponential_alpha << '\n';
    }

    std::cout << "Wrote " << options.output_path << '\n';
}

int run(const Options& options)
{
    std::ifstream input{options.input_path};

    if(!input)
    {
        fail("could not open input file: " + options.input_path);
    }

    std::ofstream output{options.output_path};

    if(!output)
    {
        fail("could not open output file: " + options.output_path);
    }

    std::string header;

    if(!std::getline(input, header))
    {
        fail("input CSV is empty");
    }

    const auto header_fields = split_csv_line(header);
    const auto columns = make_column_map(header_fields);
    const std::size_t time_column = require_column(columns, "time_s");
    const std::size_t measured_column =
        require_column(columns, "measured_value");
    const std::size_t required_field_count =
        std::max(time_column, measured_column) + 1;

    filtering::MeasurementValidator validator{
        {
            .minimum_value = options.minimum_value,
            .maximum_value = options.maximum_value,
            .maximum_rate_per_second = options.maximum_rate_per_second,
        }};

    std::optional<filtering::MovingAverage> moving_average;
    if(options.moving_average_window)
    {
        moving_average.emplace(*options.moving_average_window);
    }

    std::optional<filtering::ExponentialFilter> exponential_filter;
    if(options.exponential_alpha)
    {
        exponential_filter.emplace(*options.exponential_alpha);
    }

    output << header
           << ",validation_status,validated_value,moving_average,"
              "exponential,filter_updated\n";

    StatusCounts counts;
    std::optional<double> previous_time;
    std::string line;
    std::size_t line_number = 1;

    while(std::getline(input, line))
    {
        ++line_number;

        if(line.empty())
        {
            continue;
        }

        const auto fields = split_csv_line(line);

        if(fields.size() < required_field_count)
        {
            fail("CSV line " + std::to_string(line_number) +
                 " has too few fields");
        }

        const double time_seconds =
            parse_finite_double(fields[time_column],
                                "time_s on CSV line " +
                                    std::to_string(line_number));

        if(previous_time && time_seconds <= *previous_time)
        {
            fail("time_s values must be strictly increasing; CSV line " +
                 std::to_string(line_number));
        }
        previous_time = time_seconds;

        const std::optional<double> measured_value =
            parse_optional_measurement(fields[measured_column], line_number);

        const filtering::ValidationResult result =
            validator.validate(time_seconds, measured_value);
        counts.add(result.status);

        bool filter_updated = false;
        if(result.is_valid() && result.measured_value)
        {
            const double value = *result.measured_value;

            if(moving_average)
            {
                moving_average->update(value);
            }

            if(exponential_filter)
            {
                exponential_filter->update(value);
            }

            filter_updated = true;
        }

        output << line << ',' << filtering::to_string(result.status) << ',';

        if(result.is_valid())
        {
            write_optional_value(output, result.measured_value);
        }

        output << ',';
        if(moving_average)
        {
            write_optional_value(output, moving_average->value());
        }

        output << ',';
        if(exponential_filter)
        {
            write_optional_value(output, exponential_filter->value());
        }

        output << ',' << (filter_updated ? "true" : "false") << '\n';
    }

    print_summary(counts, options);
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        return run(parse_options(argc, argv));
    }
    catch(const std::exception& exception)
    {
        std::cerr << "filter_measurements: " << exception.what() << '\n';
        print_usage(std::cerr);
        return 1;
    }
}
