#pragma once
#include <map>
#include <string>
#include <windows.h>

// Parsed focus-step configuration for an attached lens.
struct FocusStepConfig
{
    UINT16 limit = 0;                       // total travel, in steps of size 1
    std::map<UINT16, UINT16> steps;         // step index (1..n) -> distance moved
    std::map<UINT16, UINT16> sleeps;        // step index (1..n) -> post-move sleep (ms)
};

// Parses the registry "Steps" / "Sleeps" strings. Returns false (with `error` describing why and
// `out` left empty) if the steps are malformed: empty tokens, non-numeric or non-positive values,
// or a step that works out as zero distance. Malformed sleeps are not fatal: the default of
// "*100" (100ms per step size) is used instead and `sleepsDefaulted` is set.
bool ParseFocusSteps(const std::wstring& steps, const std::wstring& sleeps, FocusStepConfig& out,
                     std::wstring& error, bool& sleepsDefaulted);
