#include "pch.h"
#include "FocusMemory.h"

void
FocusMemory::Remember(const std::wstring& deviceId, const std::wstring& lensKey, double position)
{
    if (position < 0)
    {
        Forget(deviceId);
        return;
    }

    std::lock_guard<std::mutex> guard(m_mutex);

    m_entries[deviceId] = Entry{ lensKey, position };
}

bool
FocusMemory::Recall(const std::wstring& deviceId, const std::wstring& lensKey, double& position) const
{
    std::lock_guard<std::mutex> guard(m_mutex);

    auto it = m_entries.find(deviceId);

    if (it == m_entries.end() || it->second.lensKey != lensKey)
    {
        return false;
    }

    position = it->second.position;

    return true;
}

void
FocusMemory::Forget(const std::wstring& deviceId)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    m_entries.erase(deviceId);
}

FocusMemory&
GetFocusMemory()
{
    static FocusMemory memory;

    return memory;
}
