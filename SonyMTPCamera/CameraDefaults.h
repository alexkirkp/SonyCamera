#pragma once
#include "pch.h"
#include "resource1.h"
#include <string>
#include <vector>

// Parser for CameraDefaults.ini / LegacyDefaults.ini (see the header comment in CameraDefaults.ini
// for the format). It only builds an in-memory table; it does not touch the registry, and it does
// not log, because it may run under the loader lock.

struct DefaultValue
{
    std::wstring name;      // empty = the key's default value
    bool isDword = false;
    DWORD dword = 0;        // when isDword
    std::wstring text;      // otherwise
};

struct DefaultKey
{
    std::wstring path;      // e.g. Cameras\Sony Corporation\ILCE-6000 (relative to the registry root)
    std::vector<DefaultValue> values;
};

class CameraDefaults
{
public:
    // Parses UTF-8 text. On failure returns false and sets error (line number and reason).
    static bool Parse(const std::string& text, CameraDefaults& out, std::string& error);

    // Parses a table embedded in the DLL as an RCDATA resource: IDR_CAMERA_DEFAULTS (the shipped
    // table) or IDR_LEGACY_DEFAULTS (what old builds wrote into the user layer).
    static bool LoadEmbedded(CameraDefaults& out, std::string& error, int resourceId = IDR_CAMERA_DEFAULTS);

    const std::vector<DefaultKey>& Keys() const { return m_keys; }

    // Version stamp of the parsed table: a 32-bit FNV-1a hash of the source text, so any edit to
    // the .ini changes it. Stored as Defaults\DataVersion to decide whether to rewrite the layer.
    DWORD Version() const { return m_version; }

    // One line per fact, in the form used by tools/camera_defaults.py:
    //   K<TAB>path
    //   V<TAB>path<TAB>name<TAB>S|D<TAB>text      (name is "@" for the default value)
    std::vector<std::wstring> Flatten() const;

private:
    std::vector<DefaultKey> m_keys;
    DWORD m_version = 0;
};
