#include "StreamScheduler.h"
#include <algorithm>

namespace nmea_sim
{
void StreamScheduler::addStream(std::size_t id, double hz)
{
    if (hz <= 0.0) return;
    Stream s{};
    s.id = id;
    s.hz = hz;
    s.period_sec = 1.0 / hz;
    const auto now = Clock::now();
    s.last_fire = now;
    s.next_due = now + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(s.period_sec));
    m_streams.push_back(s);
}

const StreamScheduler::Stream& StreamScheduler::nextStream() const noexcept
{
    auto it = std::min_element(m_streams.begin(), m_streams.end(),
        [](const Stream& a, const Stream& b) { return a.next_due < b.next_due; });
    return *it;
}

void StreamScheduler::markFired(std::size_t id, TimePoint now) noexcept
{
    for (auto& s : m_streams)
    {
        if (s.id == id)
        {
            s.last_fire = now;
            s.next_due = now + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(s.period_sec));
            return;
        }
    }
}
}
