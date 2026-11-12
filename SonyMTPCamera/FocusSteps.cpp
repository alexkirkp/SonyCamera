#include "pch.h"
#include "FocusSteps.h"
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <vector>

namespace
{
    std::vector<std::wstring> Split(const std::wstring& text)
    {
        std::vector<std::wstring> parts;
        size_t start = 0;

        while (true)
        {
            size_t comma = text.find(L',', start);
            std::wstring token = text.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);

            size_t b = 0, e = token.size();
            while (b < e && iswspace(token[b])) b++;
            while (e > b && iswspace(token[e - 1])) e--;
            parts.push_back(token.substr(b, e - b));

            if (comma == std::wstring::npos)
            {
                break;
            }

            start = comma + 1;
        }

        return parts;
    }

    bool ParseDouble(const std::wstring& s, double& v)
    {
        if (s.empty())
        {
            return false;
        }

        wchar_t* end = nullptr;
        v = wcstod(s.c_str(), &end);

        return end && *end == L'\0' && v == v && v > 0 && v < 1e9;
    }

    bool ParseUInt(const std::wstring& s, unsigned long& v)
    {
        if (s.empty() || !iswdigit(s[0]))
        {
            return false;
        }

        wchar_t* end = nullptr;
        v = wcstoul(s.c_str(), &end, 10);

        return end && *end == L'\0' && v <= 0xFFFF;
    }

    bool ParseSleeps(const std::wstring& sleeps, size_t count, std::map<UINT16, UINT16>& out)
    {
        std::vector<std::wstring> tokens = Split(sleeps);

        for (size_t index = 1; index <= count; index++)
        {
            const std::wstring& sl = tokens[index <= tokens.size() ? index - 1 : tokens.size() - 1];
            unsigned long n = 0;

            if (!sl.empty() && sl[0] == L'*')
            {
                if (!ParseUInt(sl.substr(1), n) || n * index > 0xFFFF)
                {
                    return false;
                }

                out[(UINT16)index] = (UINT16)(n * index);
            }
            else
            {
                if (!ParseUInt(sl, n))
                {
                    return false;
                }

                out[(UINT16)index] = (UINT16)n;
            }
        }

        return true;
    }
}

bool ParseFocusSteps(const std::wstring& steps, const std::wstring& sleeps, FocusStepConfig& out,
                     std::wstring& error, bool& sleepsDefaulted)
{
    out = FocusStepConfig();
    sleepsDefaulted = false;

    std::vector<std::wstring> tokens = Split(steps);
    std::vector<double> divisors;

    for (const auto& t : tokens)
    {
        double v = 0;

        if (!ParseDouble(t, v))
        {
            error = L"invalid focus step value '" + t + L"'";
            return false;
        }

        divisors.push_back(v);
    }

    if (divisors.size() > 255)
    {
        error = L"too many focus steps";
        return false;
    }

    // The first value is the total travel; each other step is limit / value
    if (divisors[0] < 1 || divisors[0] > 0xFFFF)
    {
        error = L"focus limit out of range";
        return false;
    }

    FocusStepConfig cfg;
    cfg.limit = (UINT16)divisors[0];

    for (size_t i = 0; i < divisors.size(); i++)
    {
        double d = (double)cfg.limit / divisors[i];

        if (d < 1 || d > 0xFFFF)
        {
            error = L"focus step " + std::to_wstring(i + 1) + L" has no usable distance";
            return false;
        }

        cfg.steps[(UINT16)(i + 1)] = (UINT16)d;
        cfg.exactSteps[(UINT16)(i + 1)] = d;
    }

    if (sleeps.empty() || !ParseSleeps(sleeps, divisors.size(), cfg.sleeps))
    {
        sleepsDefaulted = !sleeps.empty();
        cfg.sleeps.clear();
        ParseSleeps(L"*100", divisors.size(), cfg.sleeps);
    }

    out = cfg;
    return true;
}

FocusPlan PlanFocusMove(double from, double target, const std::map<UINT16, double>& sizes, UINT16 limit)
{
    FocusPlan plan;
    plan.landing = from;

    if (sizes.empty())
    {
        return plan;
    }

    if (target < 0)
    {
        target = 0;
    }

    if (target > limit)
    {
        target = limit;
    }

    const double smallest = sizes.begin()->second;
    const size_t maxNudges = sizes.size() * 10;

    // Each nudge leaves less distance than before (the closest step is always under twice the
    // distance left while that is over half the smallest step), so this ends; the cap is a backstop.
    while (std::fabs(target - plan.landing) > smallest / 2 && plan.nudges.size() < maxNudges)
    {
        double remaining = target - plan.landing;
        double distance = std::fabs(remaining);
        UINT16 best = sizes.begin()->first;
        double bestDiff = std::fabs(distance - sizes.begin()->second);

        for (const auto& s : sizes)
        {
            double diff = std::fabs(distance - s.second);

            if (diff < bestDiff)
            {
                best = s.first;
                bestDiff = diff;
            }
        }

        plan.nudges.push_back(remaining < 0 ? -(INT16)best : (INT16)best);
        plan.landing += remaining < 0 ? -sizes.at(best) : sizes.at(best);

        if (plan.landing < 0)
        {
            plan.landing = 0;
        }

        if (plan.landing > limit)
        {
            plan.landing = limit;
        }
    }

    return plan;
}
