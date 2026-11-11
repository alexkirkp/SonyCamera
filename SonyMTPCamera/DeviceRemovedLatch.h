#pragma once
#include <atomic>

// Remembers that the camera has been unplugged. Set from the device event thread, read by the
// worker and by host calls, and cleared when the device is opened again. Free of Windows types so
// it can be unit tested.
class DeviceRemovedLatch
{
public:
    void Set()            { m_removed.store(true); }
    void Clear()          { m_removed.store(false); }
    bool IsSet() const    { return m_removed.load(); }

private:
    std::atomic<bool> m_removed{ false };
};
