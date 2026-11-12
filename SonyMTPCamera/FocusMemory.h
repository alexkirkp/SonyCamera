#pragma once
#include <map>
#include <mutex>
#include <string>

// Remembers each device's last settled lens focus position for the life of the process, so a
// camera that drops off USB and is reopened (a new camera object) does not lose it: the lens does
// not move while the link is down. A position is only recalled for the same lens configuration
// (the registry steps/sleeps), so swapping lenses on the same body starts unknown.
class FocusMemory
{
public:
    void Remember(const std::wstring& deviceId, const std::wstring& lensKey, double position);
    // True (with `position` set) if a position is remembered for this device and lens
    bool Recall(const std::wstring& deviceId, const std::wstring& lensKey, double& position) const;
    void Forget(const std::wstring& deviceId);

private:
    struct Entry
    {
        std::wstring lensKey;
        double position = -1;
    };

    mutable std::mutex m_mutex;
    std::map<std::wstring, Entry> m_entries;
};

// The process-wide instance the camera objects share
FocusMemory& GetFocusMemory();
