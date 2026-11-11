#include "pch.h"
#include "CropMath.h"
#include "TestHarness.h"

// A 6000 x 4000 frame, like the a6700's RGGB output
static constexpr int W = 6000;
static constexpr int H = 4000;

TEST(Crop_NoneKeepsFullFrame)
{
    CropRect r = ResolveRawCrop(W, H, 0, 0, 0, 0);
    CHECK_EQ(r.left, 0);
    CHECK_EQ(r.top, 0);
    CHECK_EQ(r.width, W);
    CHECK_EQ(r.height, H);
    CHECK(!r.adjusted);
}

TEST(Crop_LeftCropShiftsOriginAndShrinksWidth)
{
    CropRect r = ResolveRawCrop(W, H, 0, 16, 0, 8);
    CHECK_EQ(r.left, 16);
    CHECK_EQ(r.width, W - 16 - 8);
    CHECK_EQ(r.height, H);
    CHECK(!r.adjusted);
}

TEST(Crop_AllFourEdges)
{
    CropRect r = ResolveRawCrop(W, H, 4, 10, 6, 12);
    CHECK_EQ(r.left, 10);
    CHECK_EQ(r.top, 4);
    CHECK_EQ(r.width, W - 22);
    CHECK_EQ(r.height, H - 10);
    CHECK(!r.adjusted);
}

TEST(Crop_OddLeftAndTopRoundDownToKeepBayerPhase)
{
    CropRect r = ResolveRawCrop(W, H, 3, 5, 0, 0);
    CHECK_EQ(r.left, 4);
    CHECK_EQ(r.top, 2);
    CHECK_EQ(r.width % 2, 0);
    CHECK_EQ(r.height % 2, 0);
    CHECK(r.adjusted);
}

TEST(Crop_OddWidthRoundsDownToEven)
{
    // 6000 - 0 - 7 = 5993 wide: must not write one pixel past the end of a row
    CropRect r = ResolveRawCrop(W, H, 0, 0, 0, 7);
    CHECK_EQ(r.width, 5992);
    CHECK(r.adjusted);
}

TEST(Crop_OversizedFallsBackToFullFrame)
{
    CropRect r = ResolveRawCrop(W, H, 0, 70000, 0, 0);
    CHECK_EQ(r.left, 0);
    CHECK_EQ(r.width, W);
    CHECK_EQ(r.height, H);
    CHECK(r.adjusted);

    r = ResolveRawCrop(W, H, 0, 3000, 0, 3000);   // nothing left
    CHECK_EQ(r.width, W);
    CHECK(r.adjusted);

    r = ResolveRawCrop(W, H, 2000, 0, 2000, 0);
    CHECK_EQ(r.height, H);
}

TEST(Crop_NegativeValuesTreatedAsZero)
{
    CropRect r = ResolveRawCrop(W, H, -1, -1, -1, -1);
    CHECK_EQ(r.left, 0);
    CHECK_EQ(r.width, W);
    CHECK_EQ(r.height, H);
    CHECK(r.adjusted);
}

TEST(Crop_OddRawSizeIsMadeEven)
{
    CropRect r = ResolveRawCrop(101, 51, 0, 0, 0, 0);
    CHECK_EQ(r.width, 100);
    CHECK_EQ(r.height, 50);
}

TEST(Crop_ResultAlwaysInsideFrame)
{
    for (int left = -2; left < 40; left += 3)
        for (int right = -2; right < 40; right += 5)
        {
            CropRect r = ResolveRawCrop(32, 16, 1, left, 1, right);
            CHECK(r.left >= 0 && r.top >= 0);
            CHECK(r.width >= 2 && r.height >= 2);
            CHECK(r.left + r.width <= 32);
            CHECK(r.top + r.height <= 16);
            CHECK_EQ(r.width % 2, 0);
            CHECK_EQ(r.left % 2, 0);
        }
}
