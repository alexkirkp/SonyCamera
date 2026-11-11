#pragma once

#include <windows.h>

// RAII wrapper for a timed wait on a Win32 mutex. WAIT_ABANDONED grants ownership just as
// WAIT_OBJECT_0 does (the previous owner died holding it), so both count as owned and are released
// on destruction. A timed-out or failed wait owns nothing and releases nothing.
class MutexGuard
{
public:
    MutexGuard(HANDLE mutex, DWORD timeout) : m_mutex(mutex), m_result(WaitForSingleObject(mutex, timeout)) {}
    ~MutexGuard() { Release(); }

    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

    bool Owned() const { return m_owned(); }
    explicit operator bool() const { return Owned(); }
    DWORD Result() const { return m_result; }
    bool Abandoned() const { return m_result == WAIT_ABANDONED; }

    // Release early (idempotent)
    void Release()
    {
        if (Owned() && !m_released)
        {
            ReleaseMutex(m_mutex);
            m_released = true;
        }
    }

private:
    bool m_owned() const { return m_result == WAIT_OBJECT_0 || m_result == WAIT_ABANDONED; }

    HANDLE m_mutex;
    DWORD m_result;
    bool m_released = false;
};
