#pragma once
#include "SKEE.h"

namespace OBW {

// SKEE (RaceMenu/NiOverride) BodyMorph interface, acquired from the "skee" plugin at kPostPostLoad.
// Lets the C++ side apply body morphs directly (no Papyrus per-slider round-trips, no 128-element
// array cap). Null if SKEE/RaceMenu isn't installed (then OBW falls back to the Papyrus path).
inline SKEE::IBodyMorphInterface* g_morph = nullptr;

}  // namespace OBW
