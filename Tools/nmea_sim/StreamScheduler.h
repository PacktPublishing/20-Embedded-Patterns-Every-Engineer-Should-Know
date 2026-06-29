#pragma once
#include <chrono>
#include <cstddef>
#include <vector>

namespace nmea_sim
{
class StreamScheduler
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct Stream
    {
        std::size_t id = 0;
        double hz = 0.0;
        double period_sec = 0.0;
        TimePoint next_due{};
        TimePoint last_fire{};
    };

    void addStream(std::size_t id, double hz);
    bool empty() const noexcept { return m_streams.empty(); }
    const Stream& nextStream() const noexcept;
    void markFired(std::size_t id, TimePoint now) noexcept;

private:
    std::vector<Stream> m_streams;
};
}
