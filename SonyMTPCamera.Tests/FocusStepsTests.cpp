#include "pch.h"
#include "FocusSteps.h"
#include "TestHarness.h"

namespace
{
    bool Parse(const wchar_t* steps, const wchar_t* sleeps, FocusStepConfig& cfg, bool* defaulted = nullptr)
    {
        std::wstring err;
        bool d = false;
        bool ok = ParseFocusSteps(steps, sleeps, cfg, err, d);

        if (defaulted)
        {
            *defaulted = d;
        }

        return ok;
    }
}

TEST(FocusSteps_ParsesValidList)
{
    FocusStepConfig cfg;

    CHECK(Parse(L"1000,500,100", L"", cfg));
    CHECK_EQ(cfg.limit, 1000);
    CHECK_EQ(cfg.steps.size(), (size_t)3);
    CHECK_EQ(cfg.steps[1], 1);
    CHECK_EQ(cfg.steps[2], 2);
    CHECK_EQ(cfg.steps[3], 10);
    CHECK_EQ(cfg.sleeps[1], 100);
    CHECK_EQ(cfg.sleeps[3], 300);
}

TEST(FocusSteps_SleepsMultiplierAndAbsolute)
{
    FocusStepConfig cfg;

    CHECK(Parse(L"1000,500", L"250", cfg));
    CHECK_EQ(cfg.sleeps[1], 250);
    CHECK_EQ(cfg.sleeps[2], 250);

    CHECK(Parse(L"1000,500", L"*50,400", cfg));
    CHECK_EQ(cfg.sleeps[1], 50);
    CHECK_EQ(cfg.sleeps[2], 400);
}

TEST(FocusSteps_RejectsEmptyToken)
{
    FocusStepConfig cfg;

    CHECK(!Parse(L"100,,200", L"", cfg));
    CHECK(cfg.steps.empty());
    CHECK_EQ(cfg.limit, 0);
    CHECK(!Parse(L"100,200,", L"", cfg));
    CHECK(!Parse(L",100", L"", cfg));
    CHECK(!Parse(L"", L"", cfg));
}

TEST(FocusSteps_RejectsZeroNegativeAndGarbage)
{
    FocusStepConfig cfg;

    CHECK(!Parse(L"0", L"", cfg));
    CHECK(!Parse(L"1000,0", L"", cfg));
    CHECK(!Parse(L"1000,-5", L"", cfg));
    CHECK(!Parse(L"1000,abc", L"", cfg));
    CHECK(!Parse(L"1000,5x", L"", cfg));
    // limit / value rounds down to zero distance
    CHECK(!Parse(L"1000,2000", L"", cfg));
    CHECK(!Parse(L"100000", L"", cfg));
}

TEST(FocusSteps_BadSleepsFallBackToDefault)
{
    FocusStepConfig cfg;
    bool defaulted = false;

    CHECK(Parse(L"1000,500", L"100,,200", cfg, &defaulted));
    CHECK(defaulted);
    CHECK_EQ(cfg.sleeps[1], 100);
    CHECK_EQ(cfg.sleeps[2], 200);

    CHECK(Parse(L"1000,500", L"abc", cfg, &defaulted));
    CHECK(defaulted);
    CHECK_EQ(cfg.sleeps.size(), (size_t)2);
}
