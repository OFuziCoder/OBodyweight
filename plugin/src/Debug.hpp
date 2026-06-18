#pragma once

namespace OBW {

// Master debug-logging switch (MCM "Debug logging", OFF by default). When false, the verbose
// per-NPC / per-event diagnostics (OBW_Native.Log + PresetManager info lines) are suppressed.
// Warnings and errors always log regardless. WeightManager owns the persisted value and mirrors
// it here so loggers can check it cheaply without a singleton lookup.
inline bool g_debugLog = false;

}  // namespace OBW
