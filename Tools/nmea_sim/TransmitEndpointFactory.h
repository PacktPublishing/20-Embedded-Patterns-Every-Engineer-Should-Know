#pragma once
#include "FileTransmitEndpoint.h"
#include "ITransmitEndpoint.h"
#include "Options.h"
#include "StdoutTransmitEndpoint.h"
#include "UartTransmitEndpoint.h"
#include "UdpTransmitEndpoint.h"
#include <memory>

namespace nmea_sim
{
inline std::unique_ptr<ITransmitEndpoint> create_transmitter(const Options& opt)
{
    if (opt.uart_device)
    {
        auto ep = std::make_unique<UartTransmitEndpoint>(*opt.uart_device, opt.uart_baud);
        return ep->open() ? std::move(ep) : nullptr;
    }
    if (opt.udp)
    {
        auto ep = std::make_unique<UdpTransmitEndpoint>(opt.udp->host, opt.udp->port);
        return ep->open() ? std::move(ep) : nullptr;
    }
    if (opt.file)
    {
        auto ep = std::make_unique<FileTransmitEndpoint>(*opt.file);
        return ep->open() ? std::move(ep) : nullptr;
    }
    if (opt.use_stdout)
    {
        return std::make_unique<StdoutTransmitEndpoint>();
    }
    return nullptr;
}
}
