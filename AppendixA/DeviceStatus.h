// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

enum class NetworkStatus
{
    Disconnected,
    Connected
};

class DeviceStatus
{
public:
    DeviceStatus() = default;

    void setNetworkStatus(NetworkStatus) {}

    bool networkIndicatorOn() const
    {
        return false;
    }
};
