#pragma once
#include "ITransmitEndpoint.h"
#include <fstream>
#include <string>

namespace nmea_sim
{
class FileTransmitEndpoint final : public ITransmitEndpoint
{
public:
    explicit FileTransmitEndpoint(std::string path);
    bool open() noexcept;
    bool send(std::string_view bytes) noexcept override;
private:
    std::string m_path;
    std::ofstream m_out;
};
}
