#include "Parameters.h"
#include "AnomalyDetection.h"
#include "ExponentialFilter.h"

#include <atomic>
#include <csignal>
#include <iomanip>
#include <optional>
#include <poll.h>
#include <sstream>

namespace {
std::atomic_bool running{true};
void stop(int) { running = false; }
std::string number(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}
struct Device {
    ch20::Parameters params;
    filtering::ExponentialFilter filter{0.25};
    chapter19::RateOfChangeChecker<double> rate{8.0}; // degrees C per second
    chapter19::StuckSensorChecker<double> stuck{5};
    std::optional<ch20::Clock::time_point> lastReceived;
    ch20::Clock::time_point nextReport{ch20::Clock::now()};
    std::string quality{"Invalid"}, reason{"NO_DATA"};
    std::uint32_t sequence{0};
    bool staleReported{false};
    int output{};

    void publish(std::string_view kind) const {
        const std::string value = filter.value() ? number(*filter.value()) : "NA";
        ch20::send(output, ch20::telemetryPort, std::string(kind) + "," +
            std::to_string(sequence) + "," + value + "," + quality + "," + reason);
    }
    void sensor(const ch15::ParsedSentence& msg) {
        if (msg.identifier != "WXSEN" || msg.fieldCount != 3) return;
        std::uint32_t seq{};
        double value{};
        if (!ch20::integer(msg.fields[0], seq)) return;
        const auto now = ch20::Clock::now();
        lastReceived = now;
        staleReported = false;
        sequence = seq;
        if (msg.fields[2] == "FAULT") {
            quality = "Invalid"; reason = "SENSOR_FAULT";
        } else if (msg.fields[2] != "OK" && msg.fields[2] != "JUMP" &&
                   msg.fields[2] != "RANGE" && msg.fields[2] != "STUCK") {
            quality = "Invalid"; reason = "BAD_SENSOR_MODE";
        } else if (!ch20::real(msg.fields[1], value)) {
            quality = "Invalid"; reason = "BAD_VALUE";
        } else if (!chapter19::RangeFilter<double>(-40.0, params.temperatureLimit).inRange(value)) {
            quality = "Invalid"; reason = "RANGE";
        } else {
            const bool normalRate = rate.changeNormal(value, now);
            const bool normalStuck = stuck.valueNormal(value);
            quality = normalRate && normalStuck ? "Good" : "Suspect";
            reason = !normalRate ? "RATE" : !normalStuck ? "STUCK" : "OK";
            if (quality == "Good") filter.update(value);
        }
        if (quality != "Good") publish("WXDIA");
    }
    void tick() {
        const auto now = ch20::Clock::now();
        if (!staleReported && lastReceived &&
            now - *lastReceived >= std::chrono::milliseconds(params.staleTimeout)) {
            staleReported = true;
            quality = "Suspect";
            reason = "STALE";
            publish("WXDIA");
        }
        if (now >= nextReport) {
            publish("WXTMP");
            nextReport = now + std::chrono::milliseconds(params.reportingInterval);
        }
    }
    void command(const ch15::ParsedSentence& msg, int fd, const sockaddr_in& sender,
                 const std::filesystem::path& path) {
        if (msg.identifier != "WXCMD" || msg.fieldCount < 2 || msg.fields[0].empty()) return;
        const std::string id = ch20::field(msg.fields[0]);
        std::string verdict = "ACCEPTED", detail = "OK";
        const auto op = msg.fields[1];
        if (op == "STATUS" && msg.fieldCount == 2) {
            publish("WXSTS");
        } else if (op == "PARAMETERS" && msg.fieldCount == 2) {
            ch20::send(output, ch20::telemetryPort, "WXPAR,ReportingInterval," +
                params.get("ReportingInterval") + ",TemperatureLimit," +
                params.get("TemperatureLimit") + ",StaleTimeout," + params.get("StaleTimeout"));
        } else if (op == "GET" && msg.fieldCount == 3 && !params.get(msg.fields[2]).empty()) {
            ch20::send(output, ch20::telemetryPort,
                "WXPAR," + ch20::field(msg.fields[2]) + "," + params.get(msg.fields[2]));
        } else if (op == "SET" && msg.fieldCount == 4 &&
                   params.set(msg.fields[2], msg.fields[3])) {
            if (msg.fields[2] == "ReportingInterval")
                nextReport = ch20::Clock::now() + std::chrono::milliseconds(params.reportingInterval);
        } else if (op == "SAVE" && msg.fieldCount == 2) {
            if (!ch20::writeParameters(path, params)) { verdict = "REJECTED"; detail = "SAVE_FAILED"; }
        } else {
            verdict = "REJECTED"; detail = "INVALID_COMMAND_OR_VALUE";
        }
        ch20::sendTo(fd, sender, "WXACK," + id + "," + verdict + "," + detail);
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        std::filesystem::path path = "ch20_weather.state";
        for (int i = 1; i < argc; ++i)
            if (std::string_view(argv[i]) == "--state-file" && i + 1 < argc) path = argv[++i];
        Device device;
        if (ch20::readParameters(path, device.params)) std::cout << "Restored " << path << '\n';
        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--state-file" && i + 1 < argc) { ++i; continue; }
            std::string key = arg == "--reporting-ms" ? "ReportingInterval" :
                              arg == "--temperature-limit" ? "TemperatureLimit" :
                              arg == "--stale-ms" ? "StaleTimeout" : "";
            if (key.empty() || i + 1 == argc || !device.params.set(key, argv[++i]))
                throw std::runtime_error("usage: ch20_weather_device [--state-file PATH] [--reporting-ms 100..60000] [--temperature-limit -20..125] [--stale-ms 250..60000]");
        }
        const int sensor = ch20::socketFor(ch20::sensorPort);
        const int command = ch20::socketFor(ch20::commandPort);
        device.output = command;
        std::cout << "Weather Device ready; state " << path << std::endl;
        while (running) {
            pollfd sockets[]{{sensor, POLLIN, 0}, {command, POLLIN, 0}};
            const int ready = ::poll(sockets, 2, 50);
            if (ready < 0 && errno != EINTR) throw std::runtime_error(std::strerror(errno));
            if (ready > 0 && (sockets[0].revents & POLLIN)) {
                ch15::ParsedSentence parsed{};
                if (ch20::receive(sensor, parsed)) device.sensor(parsed);
            }
            if (ready > 0 && (sockets[1].revents & POLLIN)) {
                ch15::ParsedSentence parsed{};
                sockaddr_in peer{};
                if (ch20::receive(command, parsed, &peer)) device.command(parsed, command, peer, path);
            }
            device.tick();
        }
        ::close(sensor);
        ::close(command);
    } catch (const std::exception& error) {
        std::cerr << "weather device: " << error.what() << '\n'; return 1;
    }
}
