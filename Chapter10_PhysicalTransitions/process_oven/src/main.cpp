#include "RampDriver.h"

#include <array>
#include <chrono>
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
    const auto elapsed = driver.period().count() *
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

    const bool realTime = argc == 2 && std::string_view{argv[1]} == "--realtime";

    const RampDriver::Commands startingCommands{
        20.0, 35.0, 10.0, 45.0, 55.0, 25.0, 15.0, 40.0};
    const RampDriver::Commands targetCommands{
        70.0, 80.0, 55.0, 30.0, 35.0, 75.0, 65.0, 25.0};

    auto driver = RampDriver::create(
        startingCommands, targetCommands, 5.0, 100ms);

    if (!driver)
    {
        std::cerr << "Invalid ramp-driver configuration\n";
        return 1;
    }

    std::ofstream csv{"process_oven.csv"};
    if (!csv)
    {
        std::cerr << "Unable to create process_oven.csv\n";
        return 1;
    }

    writeHeader(csv);
    writeCycle(csv, *driver);

    while (driver->activeRampers() != 0)
    {
        if (realTime)
            driver->waitForNextCycle();

        driver->step();
        writeCycle(csv, *driver);
    }

    std::cout << "Predicted cycles: " << driver->predictedCycles() << '\n'
              << "Actual cycles:    " << driver->cycle() << '\n'
              << "Nominal duration: "
              << driver->cycle() * driver->period().count() << " ms\n"
              << "CSV:              process_oven.csv\n";
}

