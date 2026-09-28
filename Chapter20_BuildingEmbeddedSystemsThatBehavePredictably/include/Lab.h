#pragma once

#include "WeatherMonitoringProtocol.h"

#include <arpa/inet.h>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <limits>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

namespace ch20 {
inline constexpr std::uint16_t sensorPort = 9520, commandPort = 9521, telemetryPort = 9522;
inline constexpr std::size_t maxDatagram = 256;
using Clock = std::chrono::steady_clock;

inline int socketFor(std::uint16_t port = 0) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error(std::strerror(errno));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof address) < 0) {
        const std::string error = std::strerror(errno);
        ::close(fd);
        throw std::runtime_error("bind port " + std::to_string(port) + ": " + error);
    }
    return fd;
}

inline sockaddr_in destination(std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return address;
}

inline bool send(int fd, std::uint16_t port, std::string_view body) {
    auto address = destination(port);
    ch15::SentenceBuffer buffer{};
    std::size_t length{};
    if (!ch15::buildSentence(body, buffer, length)) return false;
    return ::sendto(fd, buffer.data(), length, 0,
        reinterpret_cast<sockaddr*>(&address), sizeof address) == static_cast<ssize_t>(length);
}

inline bool sendTo(int fd, const sockaddr_in& address, std::string_view body) {
    ch15::SentenceBuffer buffer{};
    std::size_t length{};
    if (!ch15::buildSentence(body, buffer, length)) return false;
    return ::sendto(fd, buffer.data(), length, 0,
        reinterpret_cast<const sockaddr*>(&address), sizeof address) == static_cast<ssize_t>(length);
}

inline bool receive(int fd, ch15::ParsedSentence& parsed, sockaddr_in* sender = nullptr) {
    // ParsedSentence holds string_views into this buffer until the next receive.
    static thread_local char buffer[maxDatagram + 1]{};
    sockaddr_in peer{};
    socklen_t peerSize = sizeof peer;
    // Peek at one extra byte so oversized datagrams cannot be parsed after truncation.
    const auto length = ::recvfrom(fd, buffer, sizeof buffer, 0,
        reinterpret_cast<sockaddr*>(&peer), &peerSize);
    if (sender) *sender = peer;
    if (length <= 0 || length > static_cast<ssize_t>(maxDatagram)) return false;
    return ch15::parseSentence({buffer, static_cast<std::size_t>(length)}, parsed) == ch15::ParseStatus::Ok;
}

inline bool integer(std::string_view s, std::uint32_t& value) {
    if (s.empty()) return false;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc{} && result.ptr == s.data() + s.size();
}
inline bool real(std::string_view s, double& value) {
    if (s.empty()) return false;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc{} && result.ptr == s.data() + s.size() && std::isfinite(value);
}

inline std::string field(std::string_view value) {
    return std::string(value);
}
inline std::string qualityName(int quality) {
    switch (quality) { case 0: return "Good"; case 1: return "Suspect"; default: return "Invalid"; }
}
} // namespace ch20
