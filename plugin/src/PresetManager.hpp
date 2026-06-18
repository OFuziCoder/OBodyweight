#pragma once
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace OBW {

// Reads BodySlide SliderPresets XML on demand and turns an OBody-assigned preset into
// per-slider body-morph values driven by the OBW "mock weight" (0-100). The mock weight
// plays the role of BodySlide's weight slider, per slider:
//   - weight-supporting slider (small != big): faithful  lerp(small, big, smoothstep(w))
//   - static volume slider     (small == big): synthesized lean->full
//                                              lerp(V*(1-kLean), V, smoothstep(w))
//   - static non-volume slider               : constant V
// Values are SKEE units (xml percent / 100). Because the synthesized end only scales
// DOWN (never above the author's own value), the result never exceeds what the preset
// already defines -> no vertex overshoot is ever introduced (OBW <= OBody's values).
class PresetManager {
public:
    static PresetManager& GetSingleton();

    // Ordered slider names (original case, for NiOverride) of the named preset. Parsed and
    // cached lazily from Data\CalienteTools\BodySlide\SliderPresets. Empty if not found.
    std::vector<std::string> GetSliderNames(std::string_view a_preset);

    // Morph values (SKEE units) aligned 1:1 with GetSliderNames, computed at the given mock
    // weight (0-100). Same ordering as GetSliderNames for the same preset. (Papyrus fallback path,
    // capped at 128 by the array limit.)
    std::vector<float> GetMorphs(std::string_view a_preset, float a_mockWeight);

    // All (sliderName, value) pairs for the preset at the mock weight — UNCAPPED, for the C++
    // apply path (not bound by Papyrus's 128-element array limit). Same per-slider math as GetMorphs.
    std::vector<std::pair<std::string, float>> ComputeAll(std::string_view a_preset, float a_mockWeight);

private:
    struct Slider {
        std::string name;   // original case (NiOverride morph name)
        float       lo{0.f};  // "small" (weight 0), SKEE units
        float       hi{0.f};  // "big"   (weight 100), SKEE units
    };
    using SliderSet = std::vector<Slider>;

    const SliderSet& Ensure(std::string_view a_preset);     // parse+cache; empty set if missing
    void ParseFileInto(const std::filesystem::path& a_path); // parse every <Preset> in one file (unicode-safe)
    static bool IsVolumeSlider(const std::string& a_lowerName);

    std::unordered_map<std::string, SliderSet> _cache;  // key: trimmed+lowered preset name
    bool                                       _fullScanned{ false };
    std::recursive_mutex                       _mutex;
};

}  // namespace OBW
