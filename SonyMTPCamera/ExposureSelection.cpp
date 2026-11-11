#include "pch.h"
#include "ExposureSelection.h"
#include <cmath>

float
ExposureDWORDToFloat(DWORD exposure, float bulbValue)
{
    DWORD num = (exposure & 0xffff0000) >> 16;
    DWORD den = exposure & 0x0000ffff;
    float result = 0.0;

    switch (num)
    {
    case 0:
        result = bulbValue;
        break;

    default:
        result = (float)num / (float)den;
        break;
    }

    return round(result * 100000) / 100000;
}

DWORD
SelectExposureSetting(const std::list<DWORD>& exposureTimes, float desired)
{
    if (exposureTimes.size() < 2)
    {
        return EXPOSURE_INVALID;
    }

    auto i = exposureTimes.begin();

    i++;
    float longestNonBulb = ExposureDWORDToFloat(*i, EXPOSURE_BULB_VALUE);

    desired = desired == (float)0.0 ? EXPOSURE_BULB_VALUE : round(desired * 100000) / 100000;

    DWORD setting = EXPOSURE_INVALID;

    if (desired > longestNonBulb)
    {
        desired = EXPOSURE_BULB_VALUE;
        setting = *exposureTimes.begin();
    }

    // Find the best option
    // i already points to the longest non-bulb exposure
    // if desired is < i, then we compare the closeness to i, and i++
    // if closer to i++ move there and try again
    DWORD test1, test2;
    float fTest1, fTest2;
    float fDiff1, fDiff2;

    while (setting == EXPOSURE_INVALID && i != exposureTimes.end())
    {
        test1 = *i;
        fTest1 = ExposureDWORDToFloat(test1, EXPOSURE_BULB_VALUE);

        if (fTest1 < desired)
        {
            setting = *i;
        }

        i++;

        if (i == exposureTimes.end())
        {
            setting = test1;
        }
        else
        {
            test2 = *i;
            fTest2 = ExposureDWORDToFloat(test2, EXPOSURE_BULB_VALUE);

            fDiff1 = abs(desired - fTest1);
            fDiff2 = abs(desired - fTest2);

            if (fDiff1 < fDiff2)
            {
                setting = test1;
            }
        }
    }

    return setting;
}
