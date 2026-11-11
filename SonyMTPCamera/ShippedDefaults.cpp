#include "pch.h"
#include "ShippedDefaults.h"

static const wchar_t* const kLayer = L"Defaults";

bool
SyncShippedDefaults(Registry& reg, const CameraDefaults& table)
{
    const DWORD missing = 0;
    const DWORD stored = reg.GetDWORD(kLayer, L"DataVersion", missing);

    // Zero means "never written", so a table that happens to hash to 0 is simply rewritten.
    if (stored != missing && stored == table.Version())
    {
        return false;
    }

    reg.DeleteTree(L"Defaults\\Cameras");
    reg.DeleteTree(L"Defaults\\Lenses");

    for (const DefaultKey& key : table.Keys())
    {
        const std::wstring path = std::wstring(kLayer) + L"\\" + key.path;

        reg.CloseKey(reg.CreateKey(path));

        for (const DefaultValue& v : key.values)
        {
            if (v.isDword)
            {
                reg.SetDWORD(path, v.name, v.dword);
            }
            else
            {
                reg.SetString(path, v.name, v.text);
            }
        }
    }

    // Last, so an interrupted write is retried on the next load.
    reg.SetDWORD(kLayer, L"DataVersion", table.Version());

    return true;
}
