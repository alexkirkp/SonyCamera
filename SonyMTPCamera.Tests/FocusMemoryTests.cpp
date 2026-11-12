#include "pch.h"
#include "FocusMemory.h"
#include "TestHarness.h"

namespace
{
    const wchar_t* A6700 = L"\\\\?\\usb#vid_054c&pid_0e78#5544#{6ac27878-a6fa-4155-ba85-f98f491d4f33}";
    const wchar_t* Tamron = L"3120,1230,511,195,71.1,25.1,9.2|";
}

TEST(FocusMemory_RecallsTheLastPositionForTheSameDeviceAndLens)
{
    FocusMemory m;
    double p = -1;

    m.Remember(A6700, Tamron, 2995.7);
    m.Remember(A6700, Tamron, 3000.3);

    CHECK(m.Recall(A6700, Tamron, p));
    CHECK_EQ(p, 3000.3);
}

TEST(FocusMemory_NothingBeforeTheFirstMove)
{
    FocusMemory m;
    double p = 42;

    CHECK(!m.Recall(A6700, Tamron, p));
    CHECK_EQ(p, 42.0);
}

TEST(FocusMemory_AnotherLensStartsUnknown)
{
    FocusMemory m;
    double p = -1;

    m.Remember(A6700, Tamron, 3000);

    CHECK(!m.Recall(A6700, L"1000,100,10|", p));
}

TEST(FocusMemory_DevicesAreKeptApart)
{
    FocusMemory m;
    double p = -1;

    m.Remember(A6700, Tamron, 3000);
    m.Remember(L"other", Tamron, 100);

    CHECK(m.Recall(A6700, Tamron, p));
    CHECK_EQ(p, 3000.0);
    CHECK(m.Recall(L"other", Tamron, p));
    CHECK_EQ(p, 100.0);
}

TEST(FocusMemory_ForgetAndUnknownPositionClearIt)
{
    FocusMemory m;
    double p = -1;

    m.Remember(A6700, Tamron, 3000);
    m.Forget(A6700);
    CHECK(!m.Recall(A6700, Tamron, p));

    m.Remember(A6700, Tamron, 3000);
    m.Remember(A6700, Tamron, -1);
    CHECK(!m.Recall(A6700, Tamron, p));
}
