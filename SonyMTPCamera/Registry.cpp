#include "pch.h"
#include "Registry.h"
#include <vector>

Registry::Registry(std::wstring root)
    : m_root(root)
{

}

bool
Registry::Open()
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    HKEY key;

    m_openCount++;

    if (m_openCount == 1)
    {
        if (RegCreateKeyEx(HKEY_CURRENT_USER, m_root.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
        {
            m_hk = key;
        }
        else
        {
            m_openCount--;
        }
    }

    return m_openCount > 0;
}

void
Registry::Close()
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    if (m_openCount > 0)
    {
        m_openCount--;

        if (m_openCount == 0)
        {
            RegCloseKey(m_hk);
            m_hk = nullptr;
        }
    }
}

std::wstring
Registry::ShippedPath(const std::wstring& path)
{
    auto starts = [&path](const wchar_t* root)
    {
        size_t n = wcslen(root);

        return path.size() >= n && _wcsnicmp(path.c_str(), root, n) == 0 &&
            (path.size() == n || path[n] == L'\\');
    };

    return (starts(L"Cameras") || starts(L"Lenses")) ? L"Defaults\\" + path : std::wstring();
}

std::wstring
Registry::ReadString(const std::wstring& path, const std::wstring& value)
{
    std::wstring result;
    DWORD dataSize = 0;

    RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &dataSize);

    if (dataSize)
    {
        LPWSTR str = (LPWSTR)new BYTE[dataSize];
        RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_SZ, nullptr, str, &dataSize);

        result = std::wstring(str, dataSize / sizeof(wchar_t) - 1);
        delete[] (BYTE*)str;
    }

    return result;
}

bool
Registry::ReadDWORD(const std::wstring& path, const std::wstring& value, DWORD& result)
{
    DWORD dataSize = sizeof(DWORD);

    return RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, &result, &dataSize) == ERROR_SUCCESS;
}

std::wstring
Registry::GetString(std::wstring path, std::wstring value, std::wstring def)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    std::wstring result = ReadString(path, value);

    if (result.empty())
    {
        std::wstring shipped = ShippedPath(path);

        if (!shipped.empty())
        {
            result = ReadString(shipped, value);
        }
    }

    return result.empty() ? def : result;
}

DWORD
Registry::GetDWORD(std::wstring path, std::wstring value, DWORD def)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    DWORD result = 0;

    if (ReadDWORD(path, value, result))
    {
        return result;
    }

    std::wstring shipped = ShippedPath(path);

    return (!shipped.empty() && ReadDWORD(shipped, value, result)) ? result : def;
}

double
Registry::GetDouble(std::wstring path, std::wstring value, double def)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    double result = def;
    std::wstring strValue = GetString(path, value, L"");

    if (!strValue.empty())
    {
        result = _wtof(strValue.c_str());
    }

    return result;
}

bool
Registry::KeyExists(const std::wstring& path)
{
    HKEY hk = nullptr;
    LSTATUS hr = RegOpenKeyEx(m_hk, path.c_str(), 0, KEY_READ, &hk);

    if (hr == ERROR_SUCCESS)
    {
        RegCloseKey(hk);

        return true;
    }

    return false;
}

bool
Registry::DoesKeyExist(std::wstring path)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    if (KeyExists(path))
    {
        return true;
    }

    std::wstring shipped = ShippedPath(path);

    return !shipped.empty() && KeyExists(shipped);
}

bool
Registry::DeleteTree(std::wstring path)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    LSTATUS hr = RegDeleteTree(m_hk, path.c_str());

    return hr == ERROR_SUCCESS || hr == ERROR_FILE_NOT_FOUND;
}

bool
Registry::ReadValue(std::wstring path, std::wstring value, bool& isDword, DWORD& dword, std::wstring& text)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    DWORD type = 0;
    DWORD size = 0;

    if (RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_SZ | RRF_RT_REG_DWORD, &type, nullptr, &size) != ERROR_SUCCESS)
    {
        return false;
    }

    if (type == REG_DWORD)
    {
        size = sizeof(DWORD);
        isDword = true;
        text.clear();

        return RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, &dword, &size) == ERROR_SUCCESS;
    }

    isDword = false;
    dword = 0;

    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);

    if (RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_SZ, nullptr, buf.data(), &size) != ERROR_SUCCESS)
    {
        return false;
    }

    text = buf.data();

    return true;
}

bool
Registry::DeleteValue(std::wstring path, std::wstring value)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    HKEY hk = nullptr;

    if (RegOpenKeyEx(m_hk, path.c_str(), 0, KEY_SET_VALUE, &hk) != ERROR_SUCCESS)
    {
        return false;
    }

    LSTATUS hr = RegDeleteValue(hk, value.c_str());

    RegCloseKey(hk);

    return hr == ERROR_SUCCESS;
}

bool
Registry::DeleteKeyIfEmpty(std::wstring path)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    HKEY hk = nullptr;

    if (RegOpenKeyEx(m_hk, path.c_str(), 0, KEY_READ, &hk) != ERROR_SUCCESS)
    {
        return true;
    }

    DWORD subKeys = 0;
    DWORD values = 0;
    LSTATUS hr = RegQueryInfoKey(hk, nullptr, nullptr, nullptr, &subKeys, nullptr, nullptr, &values, nullptr, nullptr, nullptr, nullptr);

    RegCloseKey(hk);

    // RegDeleteKey fails on a key with subkeys, but check explicitly so the intent is clear.
    if (hr != ERROR_SUCCESS || subKeys != 0 || values != 0)
    {
        return false;
    }

    return RegDeleteKey(m_hk, path.c_str()) == ERROR_SUCCESS;
}

std::list<std::wstring>
Registry::GetChildKeys(std::wstring path)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    std::list<std::wstring> results = ChildKeys(path);
    std::wstring shipped = ShippedPath(path);

    if (!shipped.empty())
    {
        // Union: user keys first, then shipped keys the user layer does not already have.
        for (const std::wstring& name : ChildKeys(shipped))
        {
            bool found = false;

            for (const std::wstring& have : results)
            {
                if (_wcsicmp(have.c_str(), name.c_str()) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                results.push_back(name);
            }
        }
    }

    return results;
}

std::list<std::wstring>
Registry::ChildKeys(const std::wstring& path)
{
    std::list<std::wstring> results;

    HKEY hk = nullptr;
    LSTATUS hr = RegOpenKeyEx(m_hk, path.c_str(), 0, KEY_READ | KEY_QUERY_VALUE, &hk);

    if (hr == ERROR_SUCCESS)
    {
        DWORD index = 0;
        DWORD size = 0;

        if (RegQueryInfoKey(hk, nullptr, nullptr, nullptr, nullptr, &size, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
        {
            size += 1;
            wchar_t* name = new wchar_t[size];

            do
            {
                DWORD sizeIn = size;

                hr = RegEnumKeyEx(hk, index, name, &sizeIn, nullptr, nullptr, nullptr, nullptr);

                if (hr == ERROR_SUCCESS)
                {
                    results.push_back(name);
                    index++;
                }
            } while (hr == ERROR_SUCCESS);

            delete[] name;
        }

        RegCloseKey(hk);
    }

    return results;
}

HKEY
Registry::CreateKey(std::wstring path)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    HKEY hk = nullptr;

    RegCreateKeyEx(m_hk, path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hk, NULL);

    return hk;
}

void
Registry::CloseKey(HKEY hk)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    RegCloseKey(hk);
}

bool
Registry::SetString(HKEY hk, std::wstring value, std::wstring setting)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    return RegSetValueEx(hk, value.c_str(), 0, REG_SZ, (BYTE*)setting.c_str(), static_cast<DWORD>(setting.length() * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool
Registry::SetString(std::wstring path, std::wstring value, std::wstring setting)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    HKEY hk = CreateKey(path);

    SetString(hk, value, setting);

    CloseKey(hk);

    return true;
}

bool
Registry::SetDWORD(HKEY hk, std::wstring value, DWORD setting)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    return RegSetValueEx(hk, value.c_str(), 0, REG_DWORD, (BYTE*)&setting, sizeof(DWORD)) == ERROR_SUCCESS;
}

bool
Registry::SetDWORD(std::wstring path, std::wstring value, DWORD setting)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    HKEY hk = CreateKey(path);

    SetDWORD(hk, value, setting);

    CloseKey(hk);

    return true;
}

bool
Registry::SetDouble(std::wstring path, std::wstring value, double setting)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    std::wostringstream builder;

    builder << setting;

    std::wstring s = builder.str();

    return SetString(path, value, s);
}

bool
Registry::SetStringDefault (std::wstring path, std::wstring value, std::wstring setting)
{
    DWORD dataSize = 0;

    if (RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &dataSize) != ERROR_SUCCESS)
    {
        SetString(path, value, setting);

        return true;
    }

    return false;
}

bool
Registry::SetDWORDDefault(std::wstring path, std::wstring value, DWORD setting)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    DWORD dataSize = sizeof(DWORD);

    if (RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, nullptr, &dataSize) != ERROR_SUCCESS)
    {
        SetDWORD(path, value, setting);

        return true;
    }

    return false;
}

bool
Registry::SetDoubleDefault(std::wstring path, std::wstring value, double setting)
{
    std::lock_guard<std::recursive_mutex> guard(m_mutex);

    DWORD dataSize = 0;

    if (RegGetValue(m_hk, path.c_str(), value.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &dataSize) != ERROR_SUCCESS)
    {
        std::wostringstream builder;

        builder << setting;

        SetString(path, value, builder.str());

        return true;
    }

    return false;
}