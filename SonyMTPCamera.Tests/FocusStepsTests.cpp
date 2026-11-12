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

namespace
{
    // The Tamron 150-500mm (a057) calibration, whose second step is 2.54 units
    const wchar_t* TamronSteps = L"3120,1230,511,195,71.1,25.1,9.2";

    double SumOfNudges(double from, const FocusPlan& plan, const std::map<UINT16, double>& sizes, UINT16 limit)
    {
        double p = from;

        for (INT16 n : plan.nudges)
        {
            p += n < 0 ? -sizes.at((UINT16)-n) : sizes.at((UINT16)n);
            p = p < 0 ? 0 : (p > limit ? limit : p);
        }

        return p;
    }
}

TEST(FocusSteps_KeepsUnroundedStepSizes)
{
    FocusStepConfig cfg;

    CHECK(Parse(TamronSteps, L"", cfg));
    CHECK_EQ(cfg.steps[2], 2);
    CHECK(std::fabs(cfg.exactSteps[2] - 3120.0 / 1230) < 1e-9);
    CHECK(std::fabs(cfg.exactSteps[7] - 3120.0 / 9.2) < 1e-9);
}

TEST(FocusPlan_LandsWithinHalfAStepOfEveryTarget)
{
    FocusStepConfig cfg;
    CHECK(Parse(TamronSteps, L"", cfg));

    // Every move starts from the infinity stop (the lens resets there each time)
    for (int target = 2500; target < 3120; target++)
    {
        FocusPlan plan = PlanFocusMove(cfg.limit, target, cfg.exactSteps, cfg.limit);

        CHECK(std::fabs(plan.landing - target) <= 0.5);
        CHECK(plan.nudges.size() <= 8);
        CHECK(std::fabs(plan.landing - SumOfNudges(cfg.limit, plan, cfg.exactSteps, cfg.limit)) < 1e-9);
    }
}

TEST(FocusPlan_CountsTheTrueSizeOfASecondStep)
{
    FocusStepConfig cfg;
    CHECK(Parse(TamronSteps, L"", cfg));

    // From the stop, -6 leaves the lens at 2995.70 (rounded sizes said 2996). For 2993 the closest
    // step is then -2, which really moves 2.54: the old whole-unit count sent -6 -2 -1 and reported
    // 2993 while the lens sat at 2992.16; now it is -6 -2, reported as 2993.16.
    FocusPlan plan = PlanFocusMove(3120, 2993, cfg.exactSteps, cfg.limit);

    CHECK_EQ(plan.nudges.size(), (size_t)2);
    CHECK_EQ(plan.nudges[0], (INT16)-6);
    CHECK_EQ(plan.nudges[1], (INT16)-2);
    CHECK(std::fabs(plan.landing - (3120 - 3120.0 / 25.1 - 3120.0 / 1230)) < 1e-9);
}

TEST(FocusPlan_NoNudgesWhenAlreadyThere)
{
    FocusStepConfig cfg;
    CHECK(Parse(TamronSteps, L"", cfg));

    CHECK(PlanFocusMove(3000.3, 3000, cfg.exactSteps, cfg.limit).nudges.empty());
    CHECK(PlanFocusMove(3120, 3120, cfg.exactSteps, cfg.limit).nudges.empty());
}

TEST(FocusPlan_ClampsTargetsToTheRange)
{
    FocusStepConfig cfg;
    CHECK(Parse(TamronSteps, L"", cfg));

    FocusPlan up = PlanFocusMove(0, 9999, cfg.exactSteps, cfg.limit);
    CHECK(std::fabs(up.landing - cfg.limit) <= 0.5);

    FocusPlan down = PlanFocusMove(cfg.limit, -50, cfg.exactSteps, cfg.limit);
    CHECK(std::fabs(down.landing) <= 0.5);
}

TEST(FocusPlan_EmptyWithoutSteps)
{
    std::map<UINT16, double> none;

    FocusPlan plan = PlanFocusMove(100, 200, none, 3120);
    CHECK(plan.nudges.empty());
    CHECK_EQ(plan.landing, 100.0);
}
