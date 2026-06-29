#include "FileTransmitEndpoint.h"

namespace nmea_sim
{
FileTransmitEndpoint::FileTransmitEndpoint(std::string path) : m_path(std::move(path)) {}
bool FileTransmitEndpoint::open() noexcept
{
    m_out.open(m_path, std::ios::binary | std::ios::trunc);
    return m_out.good();
}
bool FileTransmitEndpoint::send(std::string_view bytes) noexcept
{
    m_out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return m_out.good();
}
}
