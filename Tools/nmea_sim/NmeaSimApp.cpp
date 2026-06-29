#include "NmeaSimApp.h"
#include "NmeaSentence.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace nmea_sim
{
NmeaSimApp::NmeaSimApp(const Options& opt, std::unique_ptr<ITransmitEndpoint> sink)
    : m_opt(opt)
    , m_sink(std::move(sink))
    , m_temp_gen(opt.gen, opt.seed + 1, 22.0, 5.0, 0.05, 0.02)
    , m_pressure_gen(opt.gen, opt.seed + 2, 1013.25, 2.0, 0.02, 0.10)
    , m_humidity_gen(opt.gen, opt.seed + 3, 45.0, 10.0, 0.03, 0.20)
    , m_lat_gen(opt.gen, opt.seed + 4, 43.1566, 0.001, 0.01, 0.00005)
    , m_lon_gen(opt.gen, opt.seed + 5, -77.6088, 0.001, 0.01, 0.00005)
    , m_wind_gen(opt.gen, opt.seed + 6, 5.0, 2.0, 0.07, 0.10)
    , m_fault_rng(opt.seed + 100)
{
}

void NmeaSimApp::initStreams()
{
    m_sched.addStream(Stream_Temperature, m_opt.temp_hz);
    m_sched.addStream(Stream_Pressure, m_opt.pressure_hz);
    m_sched.addStream(Stream_Humidity, m_opt.humidity_hz);
    m_sched.addStream(Stream_Position, m_opt.position_hz);
    m_sched.addStream(Stream_Wind, m_opt.wind_hz);
}

bool NmeaSimApp::sendSentence(std::string sentence) noexcept
{
    const double r = m_percent(m_fault_rng);
    if (r < m_opt.bad_checksum_percent)
    {
        sentence = corrupt_bad_checksum(std::move(sentence));
    }
    else if (r < m_opt.bad_checksum_percent + m_opt.truncated_percent)
    {
        sentence = corrupt_truncate(std::move(sentence), m_fault_rng);
    }
    return m_sink->send(sentence);
}

bool NmeaSimApp::emitTemperature(double dt_sec) noexcept
{
    std::ostringstream body;
    body << "PBOOK,TEMP," << std::fixed << std::setprecision(2) << m_temp_gen.next(dt_sec) << ",C";
    return sendSentence(make_sentence(body.str()));
}

bool NmeaSimApp::emitPressure(double dt_sec) noexcept
{
    std::ostringstream body;
    body << "PBOOK,PRESS," << std::fixed << std::setprecision(2) << m_pressure_gen.next(dt_sec) << ",hPa";
    return sendSentence(make_sentence(body.str()));
}

bool NmeaSimApp::emitHumidity(double dt_sec) noexcept
{
    std::ostringstream body;
    body << "PBOOK,HUM," << std::fixed << std::setprecision(1) << m_humidity_gen.next(dt_sec) << ",percent";
    return sendSentence(make_sentence(body.str()));
}

bool NmeaSimApp::emitPosition(double dt_sec) noexcept
{
    std::ostringstream body;
    body << "PBOOK,POS," << std::fixed << std::setprecision(6)
         << m_lat_gen.next(dt_sec) << ',' << m_lon_gen.next(dt_sec) << ",m";
    return sendSentence(make_sentence(body.str()));
}

bool NmeaSimApp::emitWind(double dt_sec) noexcept
{
    std::ostringstream body;
    body << "PBOOK,WIND," << std::fixed << std::setprecision(2) << m_wind_gen.next(dt_sec) << ",mps";
    return sendSentence(make_sentence(body.str()));
}

int NmeaSimApp::run() noexcept
{
    initStreams();
    if (m_sched.empty())
    {
        std::cerr << "No streams enabled. Set at least one stream rate.\n";
        return 2;
    }

    using Clock = StreamScheduler::Clock;
    const auto start = Clock::now();
    auto next_log = start + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(m_opt.log_every_sec));

    std::uint64_t sent = 0;
    std::uint64_t send_fail = 0;

    while (true)
    {
        const auto now = Clock::now();
        if (m_opt.duration_sec > 0.0)
        {
            const auto elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed >= m_opt.duration_sec) break;
        }

        const auto& s = m_sched.nextStream();
        if (now < s.next_due) std::this_thread::sleep_until(s.next_due);

        const auto fire = Clock::now();
        const double dt_sec = std::chrono::duration<double>(fire - s.last_fire).count();

        bool ok = false;
        switch (s.id)
        {
            case Stream_Temperature: ok = emitTemperature(dt_sec); break;
            case Stream_Pressure: ok = emitPressure(dt_sec); break;
            case Stream_Humidity: ok = emitHumidity(dt_sec); break;
            case Stream_Position: ok = emitPosition(dt_sec); break;
            case Stream_Wind: ok = emitWind(dt_sec); break;
            default: ok = false; break;
        }

        ok ? ++sent : ++send_fail;
        m_sched.markFired(s.id, fire);

        const auto after = Clock::now();
        if (!m_opt.use_stdout && after >= next_log)
        {
            std::cerr << "nmea_sim: sent=" << sent << " fail=" << send_fail << "\n";
            next_log = after + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(m_opt.log_every_sec));
        }
    }

    if (!m_opt.use_stdout) std::cerr << "nmea_sim: done\n";
    return 0;
}
}
