#include "pch.h"
#include "ExposureSelection.h"
#include "TestHarness.h"

// Exposure words: numerator in the high 16 bits, denominator in the low 16
static constexpr DWORD Exp(DWORD num, DWORD den) { return (num << 16) | den; }

// A cut-down camera list: bulb first, then longest to shortest
static const std::list<DWORD> kTimes = {
    Exp(0, 0),      // bulb
    Exp(30, 1),
    Exp(15, 1),
    Exp(1, 1),
    Exp(1, 2),
    Exp(1, 4),
    Exp(1, 1000),
    Exp(1, 4000),
};

TEST(Exposure_WordToSeconds)
{
    CHECK_EQ(ExposureDWORDToFloat(Exp(1, 2), 0.0f), 0.5f);
    CHECK_EQ(ExposureDWORDToFloat(Exp(30, 1), 0.0f), 30.0f);
    CHECK_EQ(ExposureDWORDToFloat(Exp(1, 3), 0.0f), 0.33333f);   // rounded to 5 places
    CHECK_EQ(ExposureDWORDToFloat(Exp(0, 0), 123.0f), 123.0f);   // bulb gets the caller's value
}

TEST(Exposure_ExactMatchIsChosen)
{
    CHECK_EQ(SelectExposureSetting(kTimes, 15.0f), Exp(15, 1));
    CHECK_EQ(SelectExposureSetting(kTimes, 1.0f), Exp(1, 1));
    CHECK_EQ(SelectExposureSetting(kTimes, 0.25f), Exp(1, 4));
    CHECK_EQ(SelectExposureSetting(kTimes, 30.0f), Exp(30, 1));
}

TEST(Exposure_ZeroAndTooLongMeanBulb)
{
    CHECK_EQ(SelectExposureSetting(kTimes, 0.0f), Exp(0, 0));
    CHECK_EQ(SelectExposureSetting(kTimes, 30.5f), Exp(0, 0));
    CHECK_EQ(SelectExposureSetting(kTimes, 600.0f), Exp(0, 0));
}

TEST(Exposure_InBetweenPicksTheNearerSetting)
{
    CHECK_EQ(SelectExposureSetting(kTimes, 20.0f), Exp(15, 1));   // 20 is nearer 15 than 30
    CHECK_EQ(SelectExposureSetting(kTimes, 25.0f), Exp(30, 1));   // and 25 nearer 30
    CHECK_EQ(SelectExposureSetting(kTimes, 0.4f), Exp(1, 2));
}

TEST(Exposure_ShorterThanTheFastestPicksTheFastest)
{
    CHECK_EQ(SelectExposureSetting(kTimes, 0.00001f), Exp(1, 4000));
}

TEST(Exposure_NeedsBulbPlusAtLeastOneTime)
{
    CHECK_EQ(SelectExposureSetting({}, 1.0f), EXPOSURE_INVALID);
    CHECK_EQ(SelectExposureSetting({ Exp(0, 0) }, 1.0f), EXPOSURE_INVALID);
    CHECK_EQ(SelectExposureSetting({ Exp(0, 0), Exp(1, 1) }, 1.0f), Exp(1, 1));
}
