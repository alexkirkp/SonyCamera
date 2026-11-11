#include "pch.h"
#include "CameraDefaults.h"
#include "resource1.h"
#include <map>
#include <cstdlib>
#include <cstring>

namespace
{
    struct RawValue
    {
        enum Kind { String, Dword, List, Unset } kind = String;
        std::wstring name;
        std::wstring text;      // string text, or list name
        DWORD dword = 0;
    };

    struct Section
    {
        std::wstring name;
        std::wstring inherits;
        std::vector<RawValue> values;
    };

    const int kMaxInheritDepth = 8;

    std::wstring Widen(const std::string& s)
    {
        if (s.empty())
        {
            return std::wstring();
        }

        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);

        if (n <= 0)
        {
            return std::wstring(L"�");
        }

        std::wstring w(n, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), &w[0], n);

        return w;
    }

    std::string Fail(size_t line, const char* what)
    {
        return "line " + std::to_string(line) + ": " + what;
    }

    std::string Trim(const std::string& s)
    {
        size_t b = 0;
        size_t e = s.size();

        while (b < e && (s[b] == ' ' || s[b] == '\t'))
        {
            b++;
        }

        while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
        {
            e--;
        }

        return s.substr(b, e - b);
    }

    bool StartsWith(const std::string& s, const char* prefix)
    {
        return s.compare(0, strlen(prefix), prefix) == 0;
    }

    // Parses the right-hand side of "name = value".
    bool ParseValue(const std::string& v, RawValue& out, const char*& err)
    {
        if (!v.empty() && v[0] == '"')
        {
            std::string s;
            size_t i = 1;

            for (; i < v.size() && v[i] != '"'; i++)
            {
                if (v[i] == '\\')
                {
                    if (++i >= v.size())
                    {
                        err = "unterminated escape";
                        return false;
                    }
                }

                s += v[i];
            }

            if (i >= v.size() || i + 1 != v.size())
            {
                err = "bad quoted string";
                return false;
            }

            out.kind = RawValue::String;
            out.text = Widen(s);
        }
        else if (v == "!")
        {
            out.kind = RawValue::Unset;
        }
        else if (StartsWith(v, "dword:"))
        {
            std::string digits = Trim(v.substr(6));
            char* end = nullptr;

            if (digits.empty() || digits[0] == '-')
            {
                err = "bad dword";
                return false;
            }

            unsigned long long n = strtoull(digits.c_str(), &end, 10);

            if (*end != '\0' || n > 0xFFFFFFFFull)
            {
                err = "bad dword";
                return false;
            }

            out.kind = RawValue::Dword;
            out.dword = (DWORD)n;
        }
        else if (StartsWith(v, "list:"))
        {
            out.kind = RawValue::List;
            out.text = Widen(Trim(v.substr(5)));
        }
        else
        {
            out.kind = RawValue::String;
            out.text = Widen(v);
        }

        return true;
    }

    bool StartsWithW(const std::wstring& s, const wchar_t* prefix)
    {
        return s.compare(0, wcslen(prefix), prefix) == 0;
    }

    // Applies one section's values on top of (what the parents already produced).
    bool Resolve(const std::map<std::wstring, Section>& sections, const std::map<std::wstring, std::wstring>& lists,
        const Section& s, int depth, std::vector<DefaultValue>& out, std::string& error)
    {
        if (depth > kMaxInheritDepth)
        {
            error = "inherits chain too deep or cyclic at a section";
            return false;
        }

        if (!s.inherits.empty())
        {
            auto parent = sections.find(s.inherits);

            if (parent == sections.end())
            {
                error = "section inherits an unknown section";
                return false;
            }

            if (!Resolve(sections, lists, parent->second, depth + 1, out, error))
            {
                return false;
            }
        }

        for (const RawValue& rv : s.values)
        {
            size_t at = out.size();

            for (size_t i = 0; i < out.size(); i++)
            {
                if (out[i].name == rv.name)
                {
                    at = i;
                    break;
                }
            }

            if (rv.kind == RawValue::Unset)
            {
                if (at < out.size())
                {
                    out.erase(out.begin() + at);
                }

                continue;
            }

            DefaultValue dv;
            dv.name = rv.name;

            if (rv.kind == RawValue::Dword)
            {
                dv.isDword = true;
                dv.dword = rv.dword;
            }
            else if (rv.kind == RawValue::List)
            {
                auto l = lists.find(rv.text);

                if (l == lists.end())
                {
                    error = "reference to an undefined list";
                    return false;
                }

                dv.text = l->second;
            }
            else
            {
                dv.text = rv.text;
            }

            if (at < out.size())
            {
                out[at] = dv;
            }
            else
            {
                out.push_back(dv);
            }
        }

        return true;
    }
}

bool
CameraDefaults::Parse(const std::string& text, CameraDefaults& out, std::string& error)
{
    std::vector<Section> order;
    std::map<std::wstring, Section> sections;
    std::map<std::wstring, std::wstring> lists;
    Section* current = nullptr;
    bool inLists = false;
    size_t lineNo = 0;
    size_t pos = (text.compare(0, 3, "\xEF\xBB\xBF") == 0) ? 3 : 0;     // skip a UTF-8 BOM

    out.m_keys.clear();
    out.m_version = 2166136261u;

    for (unsigned char c : text)
    {
        out.m_version = (out.m_version ^ c) * 16777619u;
    }

    while (pos <= text.size())
    {
        size_t nl = text.find('\n', pos);

        if (nl == std::string::npos)
        {
            nl = text.size();
        }

        std::string line = Trim(text.substr(pos, nl - pos));
        pos = nl + 1;
        lineNo++;

        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        if (line[0] == '[')
        {
            if (line.back() != ']' || line.size() < 3)
            {
                error = Fail(lineNo, "bad section header");
                return false;
            }

            std::wstring name = Widen(line.substr(1, line.size() - 2));

            inLists = (name == L"Lists");
            current = nullptr;

            if (!inLists)
            {
                if (sections.count(name))
                {
                    error = Fail(lineNo, "duplicate section");
                    return false;
                }

                if (!StartsWithW(name, L"Defaults.") && !StartsWithW(name, L"Cameras\\") && !StartsWithW(name, L"Lenses\\"))
                {
                    error = Fail(lineNo, "unknown section");
                    return false;
                }

                Section s;
                s.name = name;
                sections[name] = s;
                current = &sections[name];
                order.push_back(s);     // order only records names; values are read from 'sections'
            }

            continue;
        }

        size_t eq = line.find('=');

        if (eq == std::string::npos || (!inLists && current == nullptr))
        {
            error = Fail(lineNo, "expected name = value inside a section");
            return false;
        }

        std::string name = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));

        if (name.empty())
        {
            error = Fail(lineNo, "empty name");
            return false;
        }

        RawValue rv;
        const char* err = nullptr;

        if (!ParseValue(value, rv, err))
        {
            error = Fail(lineNo, err);
            return false;
        }

        if (inLists)
        {
            if (rv.kind != RawValue::String)
            {
                error = Fail(lineNo, "a list must be a string");
                return false;
            }

            if (lists.count(Widen(name)))
            {
                error = Fail(lineNo, "duplicate list");
                return false;
            }

            lists[Widen(name)] = rv.text;
            continue;
        }

        if (name == "inherits")
        {
            if (rv.kind != RawValue::String)
            {
                error = Fail(lineNo, "bad inherits");
                return false;
            }

            current->inherits = rv.text;
            continue;
        }

        rv.name = (name == "@") ? std::wstring() : Widen(name);
        current->values.push_back(rv);
    }

    for (const Section& o : order)
    {
        if (StartsWithW(o.name, L"Defaults."))
        {
            continue;
        }

        const Section& s = sections[o.name];
        DefaultKey key;
        key.path = s.name;

        if (!Resolve(sections, lists, s, 0, key.values, error))
        {
            char narrow[256];
            int n = WideCharToMultiByte(CP_UTF8, 0, s.name.c_str(), -1, narrow, sizeof(narrow), nullptr, nullptr);
            error = "[" + std::string(n > 0 ? narrow : "?") + "]: " + error;
            return false;
        }

        out.m_keys.push_back(key);
    }

    return true;
}

bool
CameraDefaults::LoadEmbedded(CameraDefaults& out, std::string& error, int resourceId)
{
    HRSRC res = FindResource(dllInstance, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    HGLOBAL mem = res ? LoadResource(dllInstance, res) : nullptr;
    const char* data = mem ? (const char*)LockResource(mem) : nullptr;

    if (data == nullptr)
    {
        error = "defaults resource not found";
        return false;
    }

    return Parse(std::string(data, SizeofResource(dllInstance, res)), out, error);
}

std::vector<std::wstring>
CameraDefaults::Flatten() const
{
    std::vector<std::wstring> lines;

    for (const DefaultKey& k : m_keys)
    {
        lines.push_back(L"K\t" + k.path);

        for (const DefaultValue& v : k.values)
        {
            lines.push_back(L"V\t" + k.path + L"\t" + (v.name.empty() ? std::wstring(L"@") : v.name) + L"\t" +
                (v.isDword ? L"D\t" + std::to_wstring(v.dword) : L"S\t" + v.text));
        }
    }

    return lines;
}
