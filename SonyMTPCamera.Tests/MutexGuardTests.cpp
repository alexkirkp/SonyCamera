#include "pch.h"
#include "MutexGuard.h"
#include "TestHarness.h"

#include <thread>

namespace
{
    // Can another thread take the mutex right now? (a mutex is owned per thread, so ask from one)
    bool OtherThreadCanTake(HANDLE mutex)
    {
        bool taken = false;
        std::thread t([&] {
            DWORD r = WaitForSingleObject(mutex, 0);
            if (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED)
            {
                taken = true;
                ReleaseMutex(mutex);
            }
        });
        t.join();
        return taken;
    }
}

TEST(MutexGuard_OwnsAndReleases)
{
    HANDLE m = CreateMutex(nullptr, FALSE, nullptr);
    {
        MutexGuard g(m, 100);
        CHECK(g.Owned());
        CHECK(!g.Abandoned());
        CHECK(!OtherThreadCanTake(m));
    }
    CHECK(OtherThreadCanTake(m));
    CloseHandle(m);
}

TEST(MutexGuard_TimeoutOwnsNothingAndReleasesNothing)
{
    HANDLE m = CreateMutex(nullptr, FALSE, nullptr);
    HANDLE held = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    HANDLE done = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    std::thread holder([&] {
        WaitForSingleObject(m, INFINITE);
        SetEvent(held);
        WaitForSingleObject(done, INFINITE);
        ReleaseMutex(m);
    });
    WaitForSingleObject(held, INFINITE);

    {
        MutexGuard g(m, 50);
        CHECK(!g.Owned());
        CHECK_EQ(g.Result(), (DWORD)WAIT_TIMEOUT);
    }

    // The guard must not have released the holder's mutex (ReleaseMutex by a non-owner would fail
    // anyway, but the holder must still own it)
    CHECK(!OtherThreadCanTake(m));

    SetEvent(done);
    holder.join();
    CHECK(OtherThreadCanTake(m));

    CloseHandle(held);
    CloseHandle(done);
    CloseHandle(m);
}

TEST(MutexGuard_AbandonedIsOwnedAndReleased)
{
    HANDLE m = CreateMutex(nullptr, FALSE, nullptr);

    // A thread that exits while holding the mutex abandons it
    std::thread t([&] { WaitForSingleObject(m, INFINITE); });
    t.join();

    {
        MutexGuard g(m, 100);
        CHECK(g.Owned());
        CHECK(g.Abandoned());
    }

    // Released again, so it is not stuck for good
    CHECK(OtherThreadCanTake(m));
    CloseHandle(m);
}

TEST(MutexGuard_EarlyReleaseIsIdempotent)
{
    HANDLE m = CreateMutex(nullptr, FALSE, nullptr);
    MutexGuard g(m, 100);
    g.Release();
    CHECK(OtherThreadCanTake(m));
    g.Release();
    CHECK(OtherThreadCanTake(m));
    CloseHandle(m);
}
