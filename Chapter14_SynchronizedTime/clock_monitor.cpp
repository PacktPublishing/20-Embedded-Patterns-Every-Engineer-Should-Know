#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace std::chrono;

static void print_wall_clock(system_clock::time_point tp)
{
    const auto seconds =
        time_point_cast<std::chrono::seconds>(tp);
    const auto ns =
        duration_cast<nanoseconds>(tp - seconds).count();

    const std::time_t t =
        system_clock::to_time_t(tp);

    std::tm tm{};
    gmtime_r(&t, &tm);

    std::cout << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << '.'
              << std::setw(9)
              << std::setfill('0')
              << ns
              << " UTC";
}

int main(int argc, char* argv[])
{
    milliseconds period{1000};

    if (argc > 1)
    {
        period = milliseconds{std::strtol(argv[1], nullptr, 10)};
    }

    const auto monotonicStart = steady_clock::now();
    auto nextSample = monotonicStart;

    std::cout
        << "Wall clock (UTC)                  "
        << "Monotonic elapsed\n";

    while (true)
    {
        const auto wallNow = system_clock::now();
        const auto steadyNow = steady_clock::now();

        const auto elapsed =
            duration_cast<milliseconds>(
                steadyNow - monotonicStart);

        print_wall_clock(wallNow);

        std::cout << "    "
                  << std::setw(10)
                  << std::setfill(' ')
                  << elapsed.count()
                  << " ms\n";

        nextSample += period;
        std::this_thread::sleep_until(nextSample);
    }
}
