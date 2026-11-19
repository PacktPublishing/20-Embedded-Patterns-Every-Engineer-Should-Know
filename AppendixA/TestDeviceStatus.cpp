#include <gtest/gtest.h>

#include "DeviceStatus.h"

TEST(DeviceStatusTests, connectedNetworkTurnsIndicatorOn)
{
    DeviceStatus deviceStatus;

    deviceStatus.setNetworkStatus(NetworkStatus::Connected);

    EXPECT_TRUE(deviceStatus.networkIndicatorOn());
}
