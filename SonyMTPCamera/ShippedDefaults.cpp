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

static void
RemoveEmptyKeys(Registry& reg, std::wstring path)
{
    // Stop at the Cameras / Lenses roots (no backslash left): those stay.
    while (path.find(L'\\') != std::wstring::npos && reg.DeleteKeyIfEmpty(path))
    {
        path.erase(path.rfind(L'\\'));
    }
}

int
MigrateLegacyDefaults(Registry& reg, const CameraDefaults& legacy)
{
    const DWORD migrated = 1;
    const DWORD missing = 0;

    if (reg.GetDWORD(kLayer, L"MigratedFrom", missing) == migrated)
    {
        return 0;
    }

    int removed = 0;

    for (const DefaultKey& key : legacy.Keys())
    {
        for (const DefaultValue& v : key.values)
        {
            bool isDword = false;
            DWORD dword = 0;
            std::wstring text;

            if (reg.ReadValue(key.path, v.name, isDword, dword, text) &&
                isDword == v.isDword &&
                (isDword ? dword == v.dword : text == v.text) &&
                reg.DeleteValue(key.path, v.name))
            {
                removed++;
            }
        }

        RemoveEmptyKeys(reg, key.path);
    }

    // Last, so an interrupted run is simply repeated (it is idempotent).
    reg.SetDWORD(kLayer, L"MigratedFrom", migrated);

    return removed;
}
