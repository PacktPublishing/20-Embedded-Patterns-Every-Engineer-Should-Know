#include "RampDriver.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace
{
constexpr std::array<std::string_view, RampDriver::zoneCount> zoneNames{
                                                                        "preheat",
                                                                        "upper_radiant",
                                                                        "lower_radiant",
                                                                        "left_wall",
                                                                        "right_wall",
                                                                        "core",
                                                                        "cure",
                                                                        "exhaust_trim"};

void writeHeader(std::ostream& output)
{
    output << "cycle,elapsed_ms,active_rampers";

    for (const auto name : zoneNames)
        output << ',' << name;

    output << '\n';
}

void writeCycle(std::ostream& output, const RampDriver& driver)
{
    const auto elapsed =
        driver.period().count() *
        static_cast<long long>(driver.cycle());

    output << driver.cycle() << ','
           << elapsed << ','
           << driver.activeRampers();

    output << std::fixed << std::setprecision(1);

    for (const double command : driver.commands())
        output << ',' << command;

    output << '\n';
}
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    bool realTime = false;

    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string_view value{argv[argument]};

        if (value == "--realtime")
        {
            if (realTime)
            {
                std::cerr
                    << "Error: --realtime was specified more than once\n";
                return 1;
            }

            realTime = true;
        }
        else
        {
            std::cerr
                << "Usage: " << argv[0] << " [--realtime]\n";
            return 1;
        }
    }

    const RampDriver::Commands startingCommands{
                                                20.0,
                                                35.0,
                                                10.0,
                                                45.0,
                                                55.0,
                                                25.0,
                                                15.0,
                                                40.0};

    const RampDriver::Commands targetCommands{
                                              70.0,
                                              80.0,
                                              55.0,
                                              30.0,
                                              35.0,
                                              75.0,
                                              65.0,
                                              25.0};

    auto driver = RampDriver::create(
        startingCommands,
        targetCommands,
        5.0,
        100ms);

    if (!driver)
    {
        std::cerr << "Invalid ramp-driver configuration\n";
        return 1;
    }

    const std::filesystem::path outputDirectory{"output"};
    const std::filesystem::path outputPath{
                                           outputDirectory / "process_oven.csv"};

    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);

    if (error)
    {
        std::cerr
            << "Unable to create output directory: "
            << error.message()
            << '\n';

        return 1;
    }

    std::ofstream output{outputPath};

    if (!output)
    {
        std::cerr
            << "Unable to open output file: "
            << outputPath
            << '\n';

        return 1;
    }

    writeHeader(output);
    writeCycle(output, *driver);

    while (driver->activeRampers() != 0)
    {
        if (realTime)
            driver->waitForNextCycle();

        driver->step();
        writeCycle(output, *driver);
    }

    std::cout
        << "Wrote process-oven results to "
        << outputPath
        << '\n';

    return 0;
}
