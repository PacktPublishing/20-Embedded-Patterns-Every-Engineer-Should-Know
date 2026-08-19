#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace std::chrono;

static void print_wall_clock(system_clock::time_point tp)
{
    const auto seconds = time_point_cast<std::chrono::seconds>(tp);
    const auto ms =
        duration_cast<milliseconds>(tp - seconds).count();

    const std::time_t t = system_clock::to_time_t(seconds);

    std::tm tm{};
    gmtime_r(&t, &tm);

    std::cout << std::put_time(&tm, "%H:%M:%S")
              << '.'
              << std::setw(3)
              << std::setfill('0')
              << ms;
}

int main(int argc, char* argv[])
{
    milliseconds period{1000};

    if (argc > 1)
    {
        period = milliseconds{
            std::strtol(argv[1], nullptr, 10)};
    }

    const auto monotonicStart = steady_clock::now();
    auto nextSample = monotonicStart;

    std::cout << "Wall UTC       Monotonic\n";

    while (true)
    {
        const auto wallNow = system_clock::now();
        const auto steadyNow = steady_clock::now();

        const auto elapsed =
            duration<double>(steadyNow - monotonicStart).count();

        print_wall_clock(wallNow);

        std::cout << "    "
                  << std::fixed
                  << std::setprecision(3)
                  << std::setw(8)
                  << std::setfill(' ')
                  << elapsed
                  << " s\n";

        nextSample += period;
        std::this_thread::sleep_until(nextSample);
    }
}
