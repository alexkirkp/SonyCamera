#pragma once
#include "pch.h"
#include "Registry.h"
#include "CameraDefaults.h"

// Writes the shipped camera/lens table into the registry's Defaults\ layer, but only when the
// table's version stamp differs from the stored Defaults\DataVersion. The layer is rewritten
// completely (stale cameras and lenses disappear). The user layer (Cameras\, Lenses\) is never
// touched. The registry must already be open. Returns true if the layer was (re)written.
bool SyncShippedDefaults(Registry& reg, const CameraDefaults& table);


// One-time migration for installs made by builds that wrote the camera and lens table straight
// into the user layer (Cameras\, Lenses\). Every user-layer value that equals the value those
// builds wrote (the frozen legacy table) is deleted, so it stops acting as an override; any other
// value is a user edit and is kept. Keys left empty are removed, as are their empty parents below
// Cameras\ and Lenses\. Guarded by Defaults\MigratedFrom, so it runs once. The registry must
// already be open. Returns the number of values removed (0 if already migrated).
int MigrateLegacyDefaults(Registry& reg, const CameraDefaults& legacy);
