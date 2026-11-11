#pragma once
#include "pch.h"
#include <list>

// Value an exposure (numerator << 16 | denominator) is reported as when its numerator is 0 (bulb)
constexpr float EXPOSURE_BULB_VALUE = 9e9f;
constexpr DWORD EXPOSURE_INVALID = 0xffffffff;

// Converts a camera exposure word (numerator in the top 16 bits, denominator in the bottom 16) to
// seconds, rounded to 5 decimal places. A zero numerator means bulb and gives bulbValue.
float ExposureDWORDToFloat(DWORD exposure, float bulbValue);

// Picks the camera exposure word closest to the desired time in seconds (0 means bulb).
// exposureTimes is the camera's list, longest first, with bulb at the front. Returns
// EXPOSURE_INVALID if the list has fewer than two entries.
DWORD SelectExposureSetting(const std::list<DWORD>& exposureTimes, float desired);
