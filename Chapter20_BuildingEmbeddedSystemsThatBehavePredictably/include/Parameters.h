#pragma once
#include "Lab.h"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <sys/stat.h>

namespace ch20 {
struct Parameters {
    std::uint32_t reportingInterval{1000};
    double temperatureLimit{45.0};
    std::uint32_t staleTimeout{3000};

    std::string get(std::string_view name) const {
        if (name == "ReportingInterval") return std::to_string(reportingInterval);
        if (name == "TemperatureLimit") return std::to_string(temperatureLimit);
        if (name == "StaleTimeout") return std::to_string(staleTimeout);
        return {};
    }
    bool set(std::string_view name, std::string_view value) {
        std::uint32_t n{};
        double d{};
        if (name == "ReportingInterval" && integer(value, n) && n >= 100 && n <= 60000) {
            reportingInterval = n; return true;
        }
        if (name == "TemperatureLimit" && real(value, d) && d >= -20.0 && d <= 125.0) {
            temperatureLimit = d; return true;
        }
        if (name == "StaleTimeout" && integer(value, n) && n >= 250 && n <= 60000) {
            staleTimeout = n; return true;
        }
        return false;
    }
};

// Compact image for this chapter's three parameters, with a version and checksum.
// The Chapter 17 image format remains the fuller example of BDS persistence.
inline std::uint32_t checksum(std::string_view content) {
    std::uint32_t sum = 2166136261U;
    for (unsigned char byte : content) { sum ^= byte; sum *= 16777619U; }
    return sum;
}
inline bool readParameters(const std::filesystem::path& path, Parameters& parameters) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    const std::string data((std::istreambuf_iterator<char>(input)), {});
    if (data.size() > 128 || data.rfind("CH20,1,", 0) != 0) return false;
    const auto last = data.rfind(',');
    if (last == std::string::npos) return false;
    std::uint32_t expected{};
    if (!integer(std::string_view(data).substr(last + 1), expected) ||
        checksum(std::string_view(data).substr(0, last)) != expected) return false;
    std::istringstream parts(data.substr(7, last - 7));
    std::string interval, limit, timeout;
    if (!std::getline(parts, interval, ',') || !std::getline(parts, limit, ',') ||
        !std::getline(parts, timeout, ',') || parts.rdbuf()->in_avail() != 0) return false;
    Parameters restored;
    if (!restored.set("ReportingInterval", interval) || !restored.set("TemperatureLimit", limit) ||
        !restored.set("StaleTimeout", timeout)) return false;
    parameters = restored;
    return true;
}
inline bool writeParameters(const std::filesystem::path& path, const Parameters& parameters) {
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."};
    const std::string body = "CH20,1," + std::to_string(parameters.reportingInterval) + "," +
        std::to_string(parameters.temperatureLimit) + "," + std::to_string(parameters.staleTimeout);
    const std::string image = body + "," + std::to_string(checksum(body));
    const auto temporary = path.string() + ".tmp";
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    const bool written = ::write(fd, image.data(), image.size()) == static_cast<ssize_t>(image.size());
    const bool flushed = written && ::fsync(fd) == 0;
    const bool closed = ::close(fd) == 0;
    if (!flushed || !closed || ::rename(temporary.c_str(), path.c_str()) != 0) {
        ::unlink(temporary.c_str()); return false;
    }
    const int dir = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0) return false;
    const bool durable = ::fsync(dir) == 0;
    ::close(dir);
    return durable;
}
} // namespace ch20
