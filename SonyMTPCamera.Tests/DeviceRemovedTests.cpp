#include "pch.h"
#include "DeviceRemovedLatch.h"
#include "TestHarness.h"

TEST(DeviceRemoved_StartsClear)
{
    DeviceRemovedLatch latch;
    CHECK(!latch.IsSet());
}

TEST(DeviceRemoved_SetIsSeen)
{
    DeviceRemovedLatch latch;
    latch.Set();
    CHECK(latch.IsSet());
    latch.Set();
    CHECK(latch.IsSet());
}

TEST(DeviceRemoved_ReopenClears)
{
    DeviceRemovedLatch latch;
    latch.Set();
    latch.Clear();
    CHECK(!latch.IsSet());
}
