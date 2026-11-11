#pragma once
#include "pch.h"
#include <list>
#include <mutex>

class Registry
{
public:
    Registry(std::wstring root);

    bool Open();
    void Close();
    std::wstring GetString(std::wstring path, std::wstring value, std::wstring def);
    DWORD GetDWORD(std::wstring path, std::wstring value, DWORD def);
    double GetDouble(std::wstring path, std::wstring value, double def);

    HKEY CreateKey(std::wstring path);
    bool DoesKeyExist(std::wstring path);
    void CloseKey(HKEY hk);
    std::list<std::wstring> GetChildKeys(std::wstring path);

    // Deletes a key and everything under it (relative to the root). True if it is gone.
    bool DeleteTree(std::wstring path);

    bool SetString(HKEY hk, std::wstring value, std::wstring setting);
    bool SetString(std::wstring path, std::wstring value, std::wstring setting);
    bool SetDWORD(HKEY hk, std::wstring value, DWORD setting);
    bool SetDWORD(std::wstring path, std::wstring value, DWORD setting);
    bool SetDouble(HKEY hk, std::wstring value, double setting);
    bool SetDouble(std::wstring path, std::wstring value, double setting);

    bool SetStringDefault(std::wstring path, std::wstring value, std::wstring setting);
    bool SetDWORDDefault(std::wstring path, std::wstring value, DWORD setting);
    bool SetDoubleDefault(std::wstring path, std::wstring value, double setting);

private:
    // Reads under Cameras\ and Lenses\ fall back from the user layer to the shipped layer at
    // Defaults\<same path>. These return the shipped path, or an empty string if not layered.
    static std::wstring ShippedPath(const std::wstring& path);
    std::wstring ReadString(const std::wstring& path, const std::wstring& value);
    bool ReadDWORD(const std::wstring& path, const std::wstring& value, DWORD& result);
    bool KeyExists(const std::wstring& path);
    std::list<std::wstring> ChildKeys(const std::wstring& path);

    HKEY m_hk = nullptr;
    std::wstring m_root;
    DWORD m_openCount = 0;
    // Guards m_hk and m_openCount. Recursive because the Set* overloads call CreateKey etc.
    std::recursive_mutex m_mutex;
};

extern Registry registry;