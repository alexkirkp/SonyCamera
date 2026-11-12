#pragma once
#include <map>
#include <string>
#include <vector>
#include <windows.h>

// Parsed focus-step configuration for an attached lens.
struct FocusStepConfig
{
    UINT16 limit = 0;                       // total travel, in steps of size 1
    std::map<UINT16, UINT16> steps;         // step index (1..n) -> distance moved, whole units
    std::map<UINT16, double> exactSteps;    // step index (1..n) -> distance moved, unrounded
    std::map<UINT16, UINT16> sleeps;        // step index (1..n) -> post-move sleep (ms)
};

// The nudges for one focus move and where they leave the lens.
struct FocusPlan
{
    std::vector<INT16> nudges;              // signed step indices, in the order to send them
    double landing = 0;                     // position after the nudges, unrounded
};

// Plans a move from `from` to `target` with the unrounded step sizes: each nudge is the step whose
// size is closest to the distance left, until no nudge would get closer (within half the smallest
// step). Positions are clamped to [0, limit], as the lens stops at its ends.
FocusPlan PlanFocusMove(double from, double target, const std::map<UINT16, double>& sizes, UINT16 limit);

// Parses the registry "Steps" / "Sleeps" strings. Returns false (with `error` describing why and
// `out` left empty) if the steps are malformed: empty tokens, non-numeric or non-positive values,
// or a step that works out as zero distance. Malformed sleeps are not fatal: the default of
// "*100" (100ms per step size) is used instead and `sleepsDefaulted` is set.
bool ParseFocusSteps(const std::wstring& steps, const std::wstring& sleeps, FocusStepConfig& out,
                     std::wstring& error, bool& sleepsDefaulted);
