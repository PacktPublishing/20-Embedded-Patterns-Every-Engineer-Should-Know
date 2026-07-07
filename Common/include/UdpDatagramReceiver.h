// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <netinet/in.h>

#include "IReadableSource.h"
#include "ImmutableByteView.h"
#include "PortStats.h"

namespace pbook
{

class UdpDatagramReceiver final : public IReadableSource
{
public:
    using Handler = void (*)(ImmutableByteView bytes,
                             const sockaddr_in& source,
                             void* userData) noexcept;

    explicit UdpDatagramReceiver(std::uint16_t port,
                                 Handler handler,
                                 void* userData = nullptr,
                                 std::size_t maxDatagramBytes = 2048);
    ~UdpDatagramReceiver() override;

    UdpDatagramReceiver(const UdpDatagramReceiver&) = delete;
    UdpDatagramReceiver& operator=(const UdpDatagramReceiver&) = delete;

    bool open() noexcept;

    int fd() const noexcept override { return m_fd; }
    void on_readable() noexcept override;

    std::uint16_t requested_port() const noexcept { return m_requestedPort; }
    std::uint16_t local_port() const noexcept { return m_localPort; }
    const PortStats& stats() const noexcept { return m_stats; }

private:
    int m_fd = -1;
    std::uint16_t m_requestedPort = 0;
    std::uint16_t m_localPort = 0;
    Handler m_handler = nullptr;
    void* m_userData = nullptr;
    std::vector<std::byte> m_buffer;
    PortStats m_stats{};
};

} // namespace pbook
