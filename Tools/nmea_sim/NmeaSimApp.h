#pragma once
#include "ITransmitEndpoint.h"
#include "Options.h"
#include "StreamScheduler.h"
#include "ValueGenerator.h"
#include <memory>
#include <random>
#include <string>

namespace nmea_sim
{
class NmeaSimApp
{
public:
    NmeaSimApp(const Options& opt, std::unique_ptr<ITransmitEndpoint> sink);
    int run() noexcept;
private:
    void initStreams();
    bool emitTemperature(double dt_sec) noexcept;
    bool emitPressure(double dt_sec) noexcept;
    bool emitHumidity(double dt_sec) noexcept;
    bool emitPosition(double dt_sec) noexcept;
    bool emitWind(double dt_sec) noexcept;
    bool sendSentence(std::string sentence) noexcept;

    Options m_opt{};
    std::unique_ptr<ITransmitEndpoint> m_sink;
    StreamScheduler m_sched{};
    ValueGenerator m_temp_gen;
    ValueGenerator m_pressure_gen;
    ValueGenerator m_humidity_gen;
    ValueGenerator m_lat_gen;
    ValueGenerator m_lon_gen;
    ValueGenerator m_wind_gen;
    std::mt19937 m_fault_rng;
    std::uniform_real_distribution<double> m_percent{0.0, 100.0};

    static constexpr std::size_t Stream_Temperature = 1;
    static constexpr std::size_t Stream_Pressure = 2;
    static constexpr std::size_t Stream_Humidity = 3;
    static constexpr std::size_t Stream_Position = 4;
    static constexpr std::size_t Stream_Wind = 5;
};
}
