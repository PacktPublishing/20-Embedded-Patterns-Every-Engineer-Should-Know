#include "Lab.h"
#include <csignal>
#include <random>
#include <thread>

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
}

int main(int argc, char** argv) {
    try {
        std::uint32_t seed = 42, interval = 500;
        bool randomFaults = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg == "--seed" && ++i < argc && ch20::integer(argv[i], seed)) continue;
            if (arg == "--interval-ms" && ++i < argc && ch20::integer(argv[i], interval) && interval >= 100) continue;
            if (arg == "--random-anomalies") { randomFaults = true; continue; }
            std::cerr << "Usage: ch20_temperature_sensor [--seed N] [--interval-ms N] [--random-anomalies]\n";
            return 2;
        }
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
        const int fd = ch20::socketFor();
        std::mt19937 rng(seed);
        std::normal_distribution<double> noise(0.0, .12);
        std::uniform_int_distribution<int> event(0, 99);
        std::uint32_t sequence{};
        double last = 20.0;
        std::cout << "Sensor: seed=" << seed << " interval=" << interval
                  << " ms; Ctrl-C creates a stale reading\n" << std::flush;
        while (running) {
            const int choice = randomFaults ? event(rng) : 100;
            std::string mode = "OK";
            double value = 20.0 + noise(rng);
            if (choice < 3) { mode = "FAULT"; }
            else if (choice < 6) { value = 150.0; mode = "RANGE"; }
            else if (choice < 9) { value = 38.0; mode = "JUMP"; }
            else if (choice < 15) { value = last; mode = "STUCK"; }
            else if (choice < 18) { mode = "SILENCE"; }
            if (mode != "SILENCE") {
                const std::string body = "WXSEN," + std::to_string(++sequence) + "," +
                    std::to_string(value) + "," + mode;
                if (!ch20::send(fd, ch20::sensorPort, body)) std::cerr << "Sensor send failed\n";
                std::cout << body << '\n' << std::flush;
                last = value;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
        ::close(fd);
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
