#include "pch.h"
#include "FocusSteps.h"
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
