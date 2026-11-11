#pragma once
#include "pch.h"
#include "Registry.h"
#include "CameraDefaults.h"

// Writes the shipped camera/lens table into the registry's Defaults\ layer, but only when the
// table's version stamp differs from the stored Defaults\DataVersion. The layer is rewritten
// completely (stale cameras and lenses disappear). The user layer (Cameras\, Lenses\) is never
// touched. The registry must already be open. Returns true if the layer was (re)written.
bool SyncShippedDefaults(Registry& reg, const CameraDefaults& table);
