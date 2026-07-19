#include "WeightManager.hpp"
#include "PresetManager.hpp"
#include "MorphInterface.hpp"
#include "Config.hpp"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>   // GetModuleHandleA (CBPC detection)
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>     // preset export

namespace OBW::PapyrusBindings {

namespace {

constexpr std::string_view kScript{ "OBW_Native" };

std::string ToLowerStr(std::string_view s) {
    std::string out{ s };
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// OBody NG procedurally varies the nipple/genital sliders per NPC and writes them under its "OBody" morph
// key. OBW's takeover clears that whole key, and OBW drives none of those sliders itself — so managed NPCs
// were losing the variation (every body got base-shape intimates). The takeover now COPIES these before the
// clear and re-writes them after (see the ApplyPresetMorphs / ApplyAllMorphs tasks). Substring match, lower
// case, covering the 3BA and BHUNP families (NippleSize/NipBGone/AreolaSize/Innieoutie/Labiapuffyness/
// Cutepuffyness/Clit*/Vagina*/Anus*...).
bool IsIntimateSlider(std::string_view a_lowerName) {
    static constexpr std::string_view kPat[] = {
        "nipple", "nipbgone", "areola", "vagina", "labia", "clit", "innie", "puffy", "anus", "pussy"
    };
    for (const auto p : kPat)
        if (a_lowerName.find(p) != std::string_view::npos) return true;
    return false;
}

// Visitor: collect every intimate slider stored under the "OBody" key (defensive to (name,key) arg order).
class IntimateCollector final : public SKEE::IBodyMorphInterface::MorphValueVisitor {
public:
    std::vector<std::pair<std::string, float>> out;
    void Visit(RE::TESObjectREFR*, const char* a_a, const char* a_b, float a_val) override {
        if (!a_a || !a_b || a_val == 0.0f) return;
        const std::string_view sa{ a_a }, sb{ a_b };
        const char* name = nullptr;
        if (sb == "OBody")      name = a_a;
        else if (sa == "OBody") name = a_b;
        if (!name) return;
        if (IsIntimateSlider(ToLowerStr(name))) out.emplace_back(name, a_val);
    }
};

// Generate and return the weight for the actor (without applying it).
float GetWeight(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GenerateWeight(a_actor);
}

// Body mode 1 (OBody preset, weight-driven): the slider names of the named OBody preset, parsed
// on demand from the BodySlide SliderPresets folder. Papyrus passes the name from
// OBodyNative.GetPresetAssignedToActor and applies each via NiOverride.SetBodyMorph.
std::vector<RE::BSFixedString> GetPresetSliders(RE::StaticFunctionTag*, RE::BSFixedString a_preset) {
    std::vector<RE::BSFixedString> out;
    try {
        const auto names = PresetManager::GetSingleton().GetSliderNames(a_preset.c_str());
        out.reserve(names.size());
        for (const auto& n : names) out.emplace_back(n.c_str());
    } catch (const std::exception& e) {
        SKSE::log::error("GetPresetSliders('{}') threw: {}", a_preset.c_str(), e.what());
    } catch (...) {
        SKSE::log::error("GetPresetSliders('{}') threw unknown exception", a_preset.c_str());
    }
    return out;
}

// The per-slider morph values for the named preset at this actor's mock weight (aligned 1:1 with
// GetPresetSliders): faithful lerp(small,big) for weight-supporting sliders, synthesized lean->full
// for static volume sliders, constant for static shape sliders. SKEE units; never exceeds the
// preset's own values (no vertex overshoot).
std::vector<float> GetPresetMorphs(RE::StaticFunctionTag*, RE::BSFixedString a_preset, RE::Actor* a_actor) {
    try {
        const float w = WeightManager::GetSingleton().GetPresetWeight(a_actor);
        return PresetManager::GetSingleton().GetMorphs(a_preset.c_str(), w);
    } catch (const std::exception& e) {
        SKSE::log::error("GetPresetMorphs('{}') threw: {}", a_preset.c_str(), e.what());
    } catch (...) {
        SKSE::log::error("GetPresetMorphs('{}') threw unknown exception", a_preset.c_str());
    }
    return {};
}

// Body mode 1 — apply the OBody-assigned preset interpolated at the actor's mock weight, ENTIRELY in
// C++ via the SKEE BodyMorph interface: sets every slider (no Papyrus 128-array cap), drops OBody's
// own "OBody"-key morphs, re-asserts the "processed" flag, and rebuilds (body + worn armor). Returns
// false if SKEE is unavailable or the preset wasn't found -> Papyrus falls back to its array path.
bool ApplyPresetMorphs(RE::StaticFunctionTag*, RE::BSFixedString a_preset, RE::Actor* a_actor,
                       RE::BSFixedString a_obKey) {
    try {
        if (!OBW::g_morph || !a_actor) return false;
        auto* task = SKSE::GetTaskInterface();
        if (!task) return false;
        const float w = WeightManager::GetSingleton().GetPresetWeight(a_actor);
        auto morphs = PresetManager::GetSingleton().ComputeAll(a_preset.c_str(), w);
        if (morphs.empty()) return false;
        const RE::FormID id = a_actor->GetFormID();
        std::string obKey = a_obKey.c_str();
        // Do ALL SKEE work on the MAIN thread (morph-store writes AND the geometry rebuild, together):
        // running it inline on the Papyrus VM thread stalls the game, and splitting the writes from the
        // rebuild across threads left the body unchanged. One task per actor; the drain throttles the
        // rate. Re-resolve the actor by FormID in case it unloaded before the task runs.
        task->AddTask([id, morphs = std::move(morphs), obKey = std::move(obKey)]() {
            if (!OBW::g_morph) return;
            auto* a = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!a) return;
            // Preserve OBody's procedural nipple/genital variation: copy it out before the clear, and skip
            // those sliders in OBW's own writes (OBody's value = preset base + its per-NPC randomization —
            // writing the preset's copy under "OBW" too would double-apply them).
            IntimateCollector keep;
            OBW::g_morph->VisitMorphValues(a, keep);
            for (const auto& [name, val] : morphs) {
                if (IsIntimateSlider(ToLowerStr(name))) continue;
                OBW::g_morph->SetMorph(a, name.c_str(), "OBW", val);
            }
            OBW::g_morph->ClearBodyMorphKeys(a, "OBody");              // remove OBody's contribution
            OBW::g_morph->SetMorph(a, obKey.c_str(), "OBody", 1.0f);   // re-assert "processed"
            for (const auto& [name, val] : keep.out)                    // restore the intimates OBody rolled
                OBW::g_morph->SetMorph(a, name.c_str(), "OBody", val);
            OBW::g_morph->ApplyBodyMorphs(a, false);                   // rebuild body + worn armor
            auto& wm = WeightManager::GetSingleton();
            wm.ApplyNeckColor(a);                                       // pull the head tint to the body tone (neck-seam color)
            wm.ScheduleNeckColor(a->GetFormID());                       // + delayed re-applies so it holds past RSV/late body
        });
        return true;
    } catch (const std::exception& e) {
        SKSE::log::error("ApplyPresetMorphs('{}') threw: {}", a_preset.c_str(), e.what());
    } catch (...) {
        SKSE::log::error("ApplyPresetMorphs('{}') threw unknown exception", a_preset.c_str());
    }
    return false;
}

// PROCEDURAL modes (0/2) — apply the ENTIRE per-NPC morph suite in ONE native call (2026-07-15 perf fix).
// The old path was ~110 Papyrus native calls per NPC (49 female sliders x get+set + male 29 x get+set),
// which saturated the Papyrus VM time-slice: bodies took seconds to apply in crowded cells and the VM
// congestion even delayed OnKeyDown (the "re-roll key feels dead" report). Now Papyrus makes ONE call;
// values are computed here (WeightManager is thread-safe) and ALL SKEE work runs in one main-thread task
// (same proven pattern as ApplyPresetMorphs above): set morphs -> oriented blend -> drop OBody's morphs ->
// re-assert "processed" -> clothed-refit delta -> ONE rebuild -> neck color. Returns false when SKEE's C++
// interface is unavailable -> Papyrus falls back to its old slider-by-slider path.
bool ApplyAllMorphs(RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_isFemale, RE::BSFixedString a_obKey) {
    try {
        if (!OBW::g_morph || !a_actor) return false;
        auto* task = SKSE::GetTaskInterface();
        if (!task) return false;
        auto& wm = WeightManager::GetSingleton();

        std::vector<std::pair<std::string, float>> morphs;
        morphs.reserve(56);
        const float kDef = wm.GetMorphScale() / 100.0f;   // shape sliders: 0-100 value -> SKEE 0-1 x master scale

        if (a_isFemale) {
            const float T = wm.GetFrameScore(a_actor);
            auto vol = [&](const char* n) { morphs.emplace_back(n, wm.GetVolumeMorph(a_actor, T, n)); };
            auto def = [&](const char* n) { morphs.emplace_back(n, wm.GetMorphValue(a_actor, T, n) * kDef); };
            // Volume (intensity + soft-cap baked in) — mirrors OBW_Quest.ApplyFemaleMorphs exactly.
            vol("Breasts"); vol("Butt"); vol("Belly"); vol("Hips"); vol("Thighs"); vol("BigButt");
            vol("Arms"); vol("ForearmSize"); vol("WristSize"); vol("ChubbyArms");
            vol("CalfSize"); vol("ThighOutsideThicc_v2"); vol("ThighFBThicc_v2"); vol("ChubbyLegs"); vol("BigBelly");
            // Definition / shape (master scale only).
            def("Waist"); def("BreastsGone"); def("BreastPerkiness"); def("BreastGravity2"); def("BreastWidth");
            def("HipBone"); def("ThighInsideThicc_v2"); def("SlimThighs");
            def("VeraMuscleTones"); def("MuscleAbs"); def("MuscleArms"); def("MuscleLegs");
            def("MuscleMoreAbs_v2"); def("MuscleMoreArms_v2"); def("MuscleMoreLegs_v2");
            def("ShoulderWidth"); def("RoundAss"); def("AppleCheeks"); def("ButtClassic"); def("ButtShape2");
            def("ButtDimples"); def("MuscleButt"); def("BreastsTogether"); def("BreastCleavage"); def("BreastHeight");
            def("WideWaistLine"); def("ChubbyWaist"); def("BigTorso"); def("ChestWidth"); def("RibsProminance");
            def("HipForward"); def("LegShapeClassic"); def("BreastTopSlope"); def("BreastSideShape");
            // BHUNP renamed thigh sliders (absent on 3BA = harmless no-op; one path drives either body).
            morphs.emplace_back("ThighInnerThicker", wm.GetMorphValue(a_actor, T, "ThighInsideThicc_v2") * kDef);
            morphs.emplace_back("ThighOuter",        wm.GetVolumeMorph(a_actor, T, "ThighOutsideThicc_v2"));
            morphs.emplace_back("ThighFBThicker",    wm.GetVolumeMorph(a_actor, T, "ThighFBThicc_v2"));
        } else {
            auto vol = [&](const char* n) { morphs.emplace_back(n, wm.GetMaleVolumeMorph(a_actor, n)); };
            auto def = [&](const char* n) { morphs.emplace_back(n, wm.GetMaleMorphValue(a_actor, n) * kDef); };
            // Volume (build; intensity + HIMBO soft-cap baked in) — mirrors OBW_Quest.ApplyMaleMorphs.
            vol("Muscle"); vol("BodyMass"); vol("PecsSize"); vol("PecsWidth");
            vol("ArmsBiceps"); vol("ArmsShoulders"); vol("ArmsTraps"); vol("ArmsFore");
            vol("TorsoBackSize"); vol("Chubby"); vol("TorsoBelly"); vol("TorsoBellyLHandles");
            vol("LegsSize"); vol("LegsThigh"); vol("LegsCalfSize"); vol("ButtBooty"); vol("ButtRoundy");
            // Definition / shape.
            def("Lean"); def("PecsFlatten"); def("TorsoShoulderInc"); def("TorsoWaistSize"); def("TorsoWidth");
            def("TorsoFlatAbs"); def("TorsoVLine"); def("TorsoRibsDefinition");
            def("TorsoBackShape"); def("TorsoBackDefinition"); def("ArmsTrapsValleys"); def("LegsThinner");
        }

        // Oriented blend strength (mode 2 only): each OBW slider is pulled toward the OBody preset value.
        // The preset's "OBody" morphs are read INSIDE the task (main thread), right before we clear them.
        const float orient = (wm.GetBodyMode() == BodyMode::kProceduralOriented) ? wm.GetPresetOrient() : 0.0f;

        const RE::FormID id = a_actor->GetFormID();
        std::string obKey = a_obKey.c_str();
        task->AddTask([id, morphs = std::move(morphs), obKey = std::move(obKey), orient]() {
            if (!OBW::g_morph) return;
            auto* a = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!a) return;
            for (const auto& [name, val] : morphs) {
                float v = val;
                if (orient > 0.0f) {
                    const float pv = OBW::g_morph->GetMorph(a, name.c_str(), "OBody");
                    if (pv > 0.0f) v = v * (1.0f - orient) + pv * orient;
                }
                OBW::g_morph->SetMorph(a, name.c_str(), "OBW", v);
            }
            float wasProcessed = OBW::g_morph->GetMorph(a, obKey.c_str(), "OBody");
            if (wasProcessed == 0.0f) wasProcessed = 1.0f;
            // Preserve OBody's procedural nipple/genital variation across the clear (OBW drives none of
            // those sliders, so without this every managed NPC reverted to base-shape intimates).
            IntimateCollector keep;
            OBW::g_morph->VisitMorphValues(a, keep);
            OBW::g_morph->ClearBodyMorphKeys(a, "OBody");              // remove OBody's contribution
            OBW::g_morph->SetMorph(a, obKey.c_str(), "OBody", wasProcessed);   // re-assert "processed"
            for (const auto& [name, val] : keep.out)                    // restore the intimates OBody rolled
                OBW::g_morph->SetMorph(a, name.c_str(), "OBody", val);
            auto& wmt = WeightManager::GetSingleton();
            wmt.ApplyClothedRefit(a, wmt.IsBodyArmorWorn(a), true, false);     // trim delta only; rebuild below
            OBW::g_morph->ApplyBodyMorphs(a, false);                   // ONE rebuild: body + worn armor
            wmt.ApplyNeckColor(a);
            wmt.ScheduleNeckColor(a->GetFormID());
        });
        return true;
    } catch (const std::exception& e) {
        SKSE::log::error("ApplyAllMorphs threw: {}", e.what());
    } catch (...) {
        SKSE::log::error("ApplyAllMorphs threw unknown exception");
    }
    return false;
}

// Debug: write a line from Papyrus into OBodyNGWeight.log — only when debug logging is ON.
void Log(RE::StaticFunctionTag*, RE::BSFixedString a_msg) {
    if (OBW::g_debugLog) SKSE::log::info("[PSC] {}", a_msg.c_str());
}

// True if the actor's source plugin is in the exclusion list (OBW should leave it alone).
bool IsExcluded(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return Config::IsActorExcluded(a_actor);
}

// MCM exclusions: the list of plugins that ADD NPCs (for the checkbox list), + get/set the
// MCM-managed exclusion of one plugin (SetPluginExcluded persists to the MCM exclusions file).
std::vector<RE::BSFixedString> GetNpcPlugins(RE::StaticFunctionTag*) {
    const auto v = Config::GetNpcPlugins();
    std::vector<RE::BSFixedString> out;
    out.reserve(v.size());
    for (const auto& n : v) out.emplace_back(n.c_str());
    return out;
}

// Paged NPC-plugin list for the MCM Exclusions page. Papyrus arrays cap at 128, so the page shows ~120 at
// a time. Config::GetNpcPlugins() is the FULL (unbounded) list; cache it so page-flips don't re-enumerate.
static std::vector<std::string>& NpcPluginCache() {
    static std::vector<std::string> cache;
    return cache;
}

std::int32_t GetNpcPluginCount(RE::StaticFunctionTag*) {
    auto& c = NpcPluginCache();
    c = Config::GetNpcPlugins();   // refresh when the page (re)builds (it calls this first)
    return static_cast<std::int32_t>(c.size());
}

std::vector<RE::BSFixedString> GetNpcPluginsPage(RE::StaticFunctionTag*, std::int32_t a_page,
                                                 std::int32_t a_perPage) {
    auto& c = NpcPluginCache();
    if (c.empty()) c = Config::GetNpcPlugins();
    std::vector<RE::BSFixedString> out;
    if (a_perPage <= 0 || a_page < 0) return out;
    const std::size_t start = static_cast<std::size_t>(a_page) * static_cast<std::size_t>(a_perPage);
    if (start >= c.size()) return out;
    std::size_t stop = start + static_cast<std::size_t>(a_perPage);
    if (stop > c.size()) stop = c.size();
    out.reserve(stop - start);
    for (std::size_t i = start; i < stop; ++i) out.emplace_back(c[i].c_str());
    return out;
}

bool IsPluginExcluded(RE::StaticFunctionTag*, RE::BSFixedString a_plugin) {
    return Config::IsPluginExcluded(a_plugin.c_str());
}

void SetPluginExcluded(RE::StaticFunctionTag*, RE::BSFixedString a_plugin, bool a_on) {
    Config::SetPluginExcluded(a_plugin.c_str(), a_on);
}

bool GetDebugLog(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetDebugLog();
}

void SetDebugLog(RE::StaticFunctionTag*, bool a_on) {
    WeightManager::GetSingleton().SetDebugLog(a_on);
}

// Mode: 0=Random, 1=Seeded/Deterministic, 2=NpcDefault
std::int32_t GetMode(RE::StaticFunctionTag*) {
    return static_cast<std::int32_t>(WeightManager::GetSingleton().GetMode());
}

void SetMode(RE::StaticFunctionTag*, std::int32_t a_mode) {
    if (a_mode < 0 || a_mode > 1) a_mode = 1;   // only Random(0)/Seeded(1) now; clamp stray values to Seeded
    WeightManager::GetSingleton().SetMode(static_cast<WeightMode>(a_mode));
}

float GetBias(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetBias();
}

void SetBias(RE::StaticFunctionTag*, float a_bias) {
    WeightManager::GetSingleton().SetBias(a_bias);
}

std::int32_t GetSeed(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetSeed();
}

void RegenerateSeed(RE::StaticFunctionTag*) {
    WeightManager::GetSingleton().RegenerateSeed();
}

std::int32_t GetBodyMode(RE::StaticFunctionTag*) {
    return static_cast<std::int32_t>(WeightManager::GetSingleton().GetBodyMode());
}

void SetBodyMode(RE::StaticFunctionTag*, std::int32_t a_mode) {
    WeightManager::GetSingleton().SetBodyMode(static_cast<BodyMode>(a_mode));
}

float GetMorphScale(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetMorphScale();
}

void SetMorphScale(RE::StaticFunctionTag*, float a_scale) {
    WeightManager::GetSingleton().SetMorphScale(a_scale);
}

// Neck-seam color fix: pull this actor's head tint toward its body tone (uses the live strength). Applies now
// AND schedules a few delayed re-applies so it holds past RSV's deferred head re-apply / the late body tone.
void NormalizeNeckColor(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    auto& wm = WeightManager::GetSingleton();
    wm.ApplyNeckColor(a_actor);
    if (a_actor) wm.ScheduleNeckColor(a_actor->GetFormID());
}
float GetNeckColorFix(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetNeckColorFix();
}
void SetNeckColorFix(RE::StaticFunctionTag*, float a_strength) {
    WeightManager::GetSingleton().SetNeckColorFix(a_strength);
}

float GetPresetOrient(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetPresetOrient();
}

void SetPresetOrient(RE::StaticFunctionTag*, float a_strength) {
    WeightManager::GetSingleton().SetPresetOrient(a_strength);
}

float GetFantasyRatio(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetFantasyRatio();
}

void SetFantasyRatio(RE::StaticFunctionTag*, float a_ratio) {
    WeightManager::GetSingleton().SetFantasyRatio(a_ratio);
}

float GetUnusualRatio(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetUnusualRatio();
}

void SetUnusualRatio(RE::StaticFunctionTag*, float a_ratio) {
    WeightManager::GetSingleton().SetUnusualRatio(a_ratio);
}

float GetBreastUnusualRatio(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetBreastUnusualRatio();
}

void SetBreastUnusualRatio(RE::StaticFunctionTag*, float a_ratio) {
    WeightManager::GetSingleton().SetBreastUnusualRatio(a_ratio);
}

float GetAthleticRatio(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetAthleticRatio();
}

void SetAthleticRatio(RE::StaticFunctionTag*, float a_ratio) {
    WeightManager::GetSingleton().SetAthleticRatio(a_ratio);
}

float GetRaceCoherence(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetRaceCoherence();
}

void SetRaceCoherence(RE::StaticFunctionTag*, float a_strength) {
    WeightManager::GetSingleton().SetRaceCoherence(a_strength);
}

float GetNaturalRatio(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetNaturalRatio();
}

void SetNaturalRatio(RE::StaticFunctionTag*, float a_ratio) {
    WeightManager::GetSingleton().SetNaturalRatio(a_ratio);
}

float GetCurvyRatio(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetCurvyRatio();
}

void SetCurvyRatio(RE::StaticFunctionTag*, float a_ratio) {
    WeightManager::GetSingleton().SetCurvyRatio(a_ratio);
}

std::int32_t GetBaseBodyPref(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetBaseBodyPref();
}

void SetBaseBodyPref(RE::StaticFunctionTag*, std::int32_t a_pref) {
    WeightManager::GetSingleton().SetBaseBodyPref(a_pref);
}

// Resolved base body (0 unknown/ambiguous, 1 CBBE, 2 BHUNP) — the MCM uses this to gate the realism toggles.
std::int32_t GetBaseBody(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetBaseBody();
}

float GetClothedRefit(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetClothedRefit();
}

void SetClothedRefit(RE::StaticFunctionTag*, float a_refit) {
    WeightManager::GetSingleton().SetClothedRefit(a_refit);
}

// Re-apply the clothed-refit delta at the actor's CURRENT worn state (force). Called by OBW_Quest right after a
// full body apply, since the "OBW" morphs were just rebuilt and the trim must be recomputed from them.
void RefreshClothedRefit(RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_rebuild) {
    if (!a_actor) return;
    auto& wm = WeightManager::GetSingleton();
    // a_rebuild=false: only set the trim delta; the caller's own ApplyBody rebuilds once (procedural path).
    wm.ApplyClothedRefit(a_actor, wm.IsBodyArmorWorn(a_actor), true, a_rebuild);
}

std::int32_t GetReRollKey(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetReRollKey();
}

void SetReRollKey(RE::StaticFunctionTag*, std::int32_t a_key) {
    WeightManager::GetSingleton().SetReRollKey(a_key);
}

bool GetFemaleBodies(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetFemaleBodies();
}

void SetFemaleBodies(RE::StaticFunctionTag*, bool a_on) {
    WeightManager::GetSingleton().SetFemaleBodies(a_on);
}

bool GetMaleBodies(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetMaleBodies();
}

void SetMaleBodies(RE::StaticFunctionTag*, bool a_on) {
    WeightManager::GetSingleton().SetMaleBodies(a_on);
}

float GetMaleBuild(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetMaleBuild();
}

void SetMaleBuild(RE::StaticFunctionTag*, float a_v) {
    WeightManager::GetSingleton().SetMaleBuild(a_v);
}

float GetActorIntensity(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetActorIntensity(a_actor);
}

std::int32_t GetPhysicsTier(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetPhysicsTier(a_actor);
}

std::int32_t GetPhysicsPercent(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_kind) {
    return WeightManager::GetSingleton().GetPhysicsPercent(a_actor, a_kind);
}

std::int32_t GetArchetypeId(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetArchetypeId(a_actor);
}

RE::BSFixedString GetArchetypeName(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetArchetypeName(a_actor);
}

RE::BSFixedString GetButtShapeName(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetButtShapeName(a_actor);
}

RE::BSFixedString GetBreastShapeName(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetBreastShapeName(a_actor);
}

// CBPC ships cbp.dll — if it's loaded, the physics integration is safe to use.
bool HasCBPC(RE::StaticFunctionTag*) {
    return GetModuleHandleA("cbp.dll") != nullptr;
}

std::int32_t ReprocessAllLoaded(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().ReprocessAllLoaded();
}

// Procedural fallback: enqueue any watched NPC OBody never handled (empty preset library / OBody-skipped).
// Returns how many were newly enqueued, so the quest can arm the drain. Polled from OBW_Quest.OnUpdate.
std::int32_t SweepFallback(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().SweepFallback();
}

void QueueForMorphs(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    WeightManager::GetSingleton().QueueForMorphs(a_actor);
}

RE::Actor* GetNextMorphActor(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetNextMorphActor();
}

bool HasMorphsPending(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().HasMorphsPending();
}

float GetFrameScore(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetFrameScore(a_actor);
}

float GetMorphValue(RE::StaticFunctionTag*, RE::Actor* a_actor, float a_frameScore, RE::BSFixedString a_name) {
    const char* raw = a_name.c_str();
    return WeightManager::GetSingleton().GetMorphValue(a_actor, a_frameScore, raw ? raw : "");
}

float GetVolumeMorph(RE::StaticFunctionTag*, RE::Actor* a_actor, float a_frameScore, RE::BSFixedString a_name) {
    const char* raw = a_name.c_str();
    return WeightManager::GetSingleton().GetVolumeMorph(a_actor, a_frameScore, raw ? raw : "");
}

float GetMaleMorphValue(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_name) {
    const char* raw = a_name.c_str();
    return WeightManager::GetSingleton().GetMaleMorphValue(a_actor, raw ? raw : "");
}

float GetMaleIntensity(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetMaleIntensity(a_actor);
}

float GetMaleVolumeMorph(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_name) {
    const char* raw = a_name.c_str();
    return WeightManager::GetSingleton().GetMaleVolumeMorph(a_actor, raw ? raw : "");
}

std::int32_t GetMaleArchetypeId(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetMaleArchetypeId(a_actor);
}

std::int32_t GetMalePhysicsPercent(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_kind) {
    return WeightManager::GetSingleton().GetMalePhysicsPercent(a_actor, a_kind);
}

RE::BSFixedString GetMaleArchetypeName(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetMaleArchetypeName(a_actor);
}

void RegenerateActor(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    WeightManager::GetSingleton().RegenerateActor(a_actor);
}

std::int32_t GetToneScore(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GetToneScore(a_actor);
}

// True only when running on the Skyrim VR runtime.
bool IsVR(RE::StaticFunctionTag*) {
    return REL::Module::IsVR();
}

// VR re-roll targeting. Skyrim VR has no traditional crosshair (GetCurrentCrosshairRef
// returns None), so instead we cone-cast from the HMD gaze and return the actor the player
// is looking at (closest within a ~25° cone), or null. Returns null outside VR so the
// cone-cast never runs in desktop play.
RE::Actor* GetVRLookTarget(RE::StaticFunctionTag*) {
    if (!REL::Module::IsVR()) return nullptr;

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* camera = RE::PlayerCamera::GetSingleton();
    if (!player || !camera || !camera->cameraRoot) return nullptr;
    auto* root = camera->cameraRoot.get();
    if (!root) return nullptr;

    const RE::NiPoint3 origin = root->world.translate;
    const auto& m = root->world.rotate;                       // forward = local +Y column
    RE::NiPoint3 fwd{ m.entry[0][1], m.entry[1][1], m.entry[2][1] };
    const float len = fwd.Length();
    if (len < 1e-4f) return nullptr;
    fwd /= len;

    auto* lists = RE::ProcessLists::GetSingleton();
    if (!lists) return nullptr;

    RE::Actor* best = nullptr;
    float bestDot = 0.90f;                                    // ~25° cone
    constexpr float kMaxDist = 2048.0f;
    for (auto& handle : lists->highActorHandles) {
        auto ptr = handle.get();
        RE::Actor* act = ptr.get();
        if (!act || act == player || act->IsDead() || act->IsDisabled() || !act->Is3DLoaded())
            continue;
        RE::NiPoint3 pos = act->GetPosition();
        pos.z += 90.0f;                                       // aim at the torso, not the feet
        RE::NiPoint3 to = pos - origin;
        const float d = to.Length();
        if (d > kMaxDist || d < 1.0f) continue;
        to /= d;
        const float dot = fwd.Dot(to);
        if (dot > bestDot) { bestDot = dot; best = act; }
    }
    return best;
}

// ── C++ hotkeys (2026-07-15) ─────────────────────────────────────────────────────────────────────
// The re-roll + exclude keys moved from Papyrus RegisterForKey to a C++ input sink. Two reasons:
// (1) reliability — OnPlayerLoadGame never fires on Quest scripts, and OBW_MCM.OnGameReload only armed
//     the poll (never re-called BindReRollKey), so on some saves the key registration was simply gone
//     ("OBW key not working"); a C++ sink works the instant a save loads, always.
// (2) responsiveness — key handling no longer queues behind a congested Papyrus VM (the morph drain).
// Papyrus OBW_Quest.OnKeyDown is now a no-op stub; the MCM rebind natives keep updating the config the
// sink reads live, so rebinding needs no re-registration at all. Sink returns kContinue ALWAYS.
class KeyInputSink final : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static KeyInputSink* GetSingleton() {
        static KeyInputSink s;
        return &s;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                          RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;
        auto* ui = RE::UI::GetSingleton();
        if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME))
            return RE::BSEventNotifyControl::kContinue;   // no game-world hotkeys in menus/console

        auto& wm = WeightManager::GetSingleton();
        const int reRoll  = wm.GetReRollKey();
        const int exclude = Config::g_excludeKey;
        const int exportK = Config::g_exportKey;

        for (RE::InputEvent* e = *a_event; e; e = e->next) {
            const auto* btn = e->AsButtonEvent();
            if (!btn || !btn->IsDown() || btn->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;
            const int key = static_cast<int>(btn->GetIDCode());
            if (key == exclude && exclude != 0)      HandleExclude();
            else if (key == exportK && exportK != 0) HandleExport();
            else if (key == reRoll && reRoll != 0)   HandleReRoll();
        }
        return RE::BSEventNotifyControl::kContinue;
    }

private:
    // Desktop: crosshair target; VR: HMD-gaze cone-cast (same helper the Papyrus native used).
    static RE::Actor* PickTarget() {
        if (REL::Module::IsVR()) return GetVRLookTarget(nullptr);
        if (auto* pick = RE::CrosshairPickData::GetSingleton()) {
            if (auto ptr = pick->targetActor.get(); ptr && ptr->As<RE::Actor>()) return ptr->As<RE::Actor>();
            if (auto ptr = pick->target.get(); ptr && ptr->As<RE::Actor>())      return ptr->As<RE::Actor>();
        }
        return nullptr;
    }

    // Ask OBW_Quest to drain the morph queue NOW (instead of waiting for the 2s poll).
    static void ArmDrain() {
        if (auto* src = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent ev{ "OBW_Drain", RE::BSFixedString{}, 0.0f, nullptr };
            src->SendEvent(&ev);
        }
    }

    static void HandleReRoll() {
        RE::Actor* a = PickTarget();
        // NO player fallback (2026-07-17). "Aim at nothing = re-roll yourself" was a footgun: one stray
        // press without an NPC under the crosshair silently gave the PLAYER an OBW body — the repeated
        // "OBW is changing my character on its own" reports. The player's body belongs to OBody's own
        // menu; OBW never writes to him, full stop.
        if (!a || a == RE::PlayerCharacter::GetSingleton()) {
            RE::DebugNotification("OBW: aim at an NPC to re-roll (your own body is set through OBody's menu).");
            return;
        }
        if (OBW::g_debugLog) {
            const std::string msg = std::string("Regenerating body: ") + a->GetDisplayFullName();
            RE::DebugNotification(msg.c_str());
        }
        WeightManager::GetSingleton().RegenerateActor(a);
        ArmDrain();
    }

    // Export the target's applied OBW body as a BodySlide preset .xml (aim at no one = export yourself).
    // Collects EVERY morph stored under OBW's "OBW" key via SKEE's visitor (so it captures procedural AND
    // preset-mode bodies, all sliders), and writes a SliderPresets file BodySlide picks up directly (in MO2
    // the write lands in overwrite). small == big -> the preset reproduces this exact body at any weight.
    class OBWMorphCollector final : public SKEE::IBodyMorphInterface::MorphValueVisitor {
    public:
        std::vector<std::pair<std::string, float>> out;
        void Visit(RE::TESObjectREFR*, const char* a_a, const char* a_b, float a_val) override {
            if (!a_a || !a_b || a_val == 0.0f) return;
            // Defensive to (name,key) argument order: one of the two is the morph KEY — ours is "OBW".
            const std::string_view sa{ a_a }, sb{ a_b };
            if (sb == "OBW")      out.emplace_back(sa, a_val);
            else if (sa == "OBW") out.emplace_back(sb, a_val);
        }
    };

    static void HandleExport() {
        RE::Actor* a = PickTarget();
        if (!a) a = RE::PlayerCharacter::GetSingleton();
        if (!a || !OBW::g_morph) return;
        const RE::FormID id = a->GetFormID();
        auto* task = SKSE::GetTaskInterface();
        if (!task) return;
        task->AddTask([id]() {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!actor || !OBW::g_morph) return;

            OBWMorphCollector col;
            OBW::g_morph->VisitMorphValues(actor, col);
            if (col.out.empty()) {
                std::string msg = std::string("OBW: no OBW body on ") + actor->GetDisplayFullName() + " to export.";
                RE::DebugNotification(msg.c_str());
                return;
            }
            std::sort(col.out.begin(), col.out.end());

            // Male body if any HIMBO-only slider is present; else female (3BA/BHUNP).
            bool male = false;
            for (const auto& [n, v] : col.out) {
                const std::string ln = ToLowerStr(n);
                if (ln == "pecssize" || ln == "muscle" || ln == "torsobelly" || ln == "bodymass") { male = true; break; }
            }

            // Sanitized preset/file name: "OBW - <actor> (<formid>)".
            std::string who = actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "Actor";
            std::string safe;
            for (char c : who)
                if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_') safe += c;
            if (safe.empty()) safe = "Actor";
            char idbuf[16];
            std::snprintf(idbuf, sizeof(idbuf), "%08X", id);
            const std::string preset = "OBW - " + safe + " (" + idbuf + ")";

            std::error_code ec;
            const std::filesystem::path dir{ R"(.\Data\CalienteTools\BodySlide\SliderPresets)" };
            std::filesystem::create_directories(dir, ec);
            const std::filesystem::path file = dir / (preset + ".xml");

            std::ofstream out(file, std::ios::trunc);
            if (!out) {
                RE::DebugNotification("OBW: export FAILED (could not write the preset file).");
                SKSE::log::error("Export: cannot open '{}'", file.string());
                return;
            }
            out << "<SliderPresets>\n";
            out << "\t<Preset name=\"" << preset << "\" set=\""
                << (male ? "HIMBO" : "CBBE 3BBB Body Amazing") << "\">\n";
            if (male) {
                out << "\t\t<Group name=\"HIMBO\"/>\n";
            } else {
                for (const char* g : { "3BA", "3BBB", "CBBE", "CBBE Bodies",
                                       "CBBE Vanilla Outfits", "CBBE Vanilla Outfits Physics", "BHUNP 3BBB" })
                    out << "\t\t<Group name=\"" << g << "\"/>\n";
            }
            for (const auto& [name, val] : col.out) {
                const int pct = static_cast<int>(std::lround(val * 100.0f));
                if (pct == 0) continue;
                out << "\t\t<SetSlider name=\"" << name << "\" size=\"big\" value=\""   << pct << "\"/>\n";
                out << "\t\t<SetSlider name=\"" << name << "\" size=\"small\" value=\"" << pct << "\"/>\n";
            }
            out << "\t</Preset>\n</SliderPresets>\n";
            out.close();

            std::string msg = std::string("OBW: exported '") + preset + "' to BodySlide presets.";
            RE::DebugNotification(msg.c_str());
            SKSE::log::info("Export: wrote {} sliders to '{}'", col.out.size(), file.string());
        });
    }

    static void HandleExclude() {
        RE::Actor* a = PickTarget();
        if (!a || a == RE::PlayerCharacter::GetSingleton()) {
            RE::DebugNotification("OBW: aim at an NPC to exclude or include it.");
            return;
        }
        const bool nowExcl = !Config::IsActorExcluded(a);
        if (!Config::SetActorExcluded(a, nowExcl)) {
            // Fully runtime-spawned NPC (dynamic base AND reference): no durable FormID to persist.
            RE::DebugNotification("OBW: this NPC is dynamically spawned and can't be excluded by ID - exclude its source mod in the MCM instead.");
            return;
        }
        std::string msg = a->GetDisplayFullName();
        if (nowExcl) {
            msg += " excluded from OBW (reload to revert its body).";
        } else {
            msg += " included in OBW.";
            WeightManager::GetSingleton().RegenerateActor(a);
            ArmDrain();
        }
        RE::DebugNotification(msg.c_str());
    }
};

void MarkMorphsApplied(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    if (a_actor) WeightManager::GetSingleton().MarkMorphsApplied(a_actor->GetFormID());
}

// OBody's apply method: re-apply our body morphs (key "OBW") to the actor's body AND worn armor via SKEE,
// DEFERRING the model rebuild to the engine's next update (deferUpdate=true). No armor unequip/re-equip and
// no synchronous re-skin -> none of the cell-entry stutter / visible morph pop those caused. No-op without
// SKEE/RaceMenu (g_morph null); the Papyrus UpdateModelWeight path stays as the fallback for that case.
void ApplyBody(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    if (a_actor && g_morph) {
        g_morph->ApplyBodyMorphs(a_actor, true);
    }
}

// Exclude / re-include a SPECIFIC NPC by FormID (runtime toggle - for a hotkey or an MCM "exclude this target").
void SetActorExcluded(RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_on) {
    Config::SetActorExcluded(a_actor, a_on);
}

// Per-NPC exclusion hotkey (aim + key). Stored in the INI via Config.
std::int32_t GetExcludeKey(RE::StaticFunctionTag*) {
    return Config::g_excludeKey;
}
void SetExcludeKey(RE::StaticFunctionTag*, std::int32_t a_key) {
    Config::SetExcludeKey(a_key);
}

// Body-preset export hotkey (aim + press -> BodySlide SliderPresets .xml). INI-persisted.
std::int32_t GetExportKey(RE::StaticFunctionTag*) {
    return Config::g_exportKey;
}
void SetExportKey(RE::StaticFunctionTag*, std::int32_t a_key) {
    Config::SetExportKey(a_key);
}

bool HasMorphsApplied(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    if (!a_actor) return false;
    return WeightManager::GetSingleton().HasMorphsApplied(a_actor->GetFormID());
}

}  // namespace

bool Register(RE::BSScript::IVirtualMachine* a_vm) {
    a_vm->RegisterFunction("GetWeight",           kScript, GetWeight);
    a_vm->RegisterFunction("GetPresetSliders",    kScript, GetPresetSliders);
    a_vm->RegisterFunction("GetPresetMorphs",     kScript, GetPresetMorphs);
    a_vm->RegisterFunction("ApplyPresetMorphs",   kScript, ApplyPresetMorphs);
    a_vm->RegisterFunction("ApplyAllMorphs",      kScript, ApplyAllMorphs);
    a_vm->RegisterFunction("Log",                 kScript, Log);
    a_vm->RegisterFunction("GetDebugLog",         kScript, GetDebugLog);
    a_vm->RegisterFunction("SetDebugLog",         kScript, SetDebugLog);
    a_vm->RegisterFunction("IsExcluded",          kScript, IsExcluded);
    a_vm->RegisterFunction("GetNpcPlugins",       kScript, GetNpcPlugins);
    a_vm->RegisterFunction("GetNpcPluginCount",   kScript, GetNpcPluginCount);
    a_vm->RegisterFunction("GetNpcPluginsPage",   kScript, GetNpcPluginsPage);
    a_vm->RegisterFunction("IsPluginExcluded",    kScript, IsPluginExcluded);
    a_vm->RegisterFunction("SetPluginExcluded",   kScript, SetPluginExcluded);
    a_vm->RegisterFunction("GetMode",             kScript, GetMode);
    a_vm->RegisterFunction("SetMode",             kScript, SetMode);
    a_vm->RegisterFunction("GetBias",             kScript, GetBias);
    a_vm->RegisterFunction("SetBias",             kScript, SetBias);
    a_vm->RegisterFunction("GetSeed",             kScript, GetSeed);
    a_vm->RegisterFunction("RegenerateSeed",      kScript, RegenerateSeed);
    a_vm->RegisterFunction("QueueForMorphs",      kScript, QueueForMorphs);
    a_vm->RegisterFunction("ReprocessAllLoaded",  kScript, ReprocessAllLoaded);
    a_vm->RegisterFunction("SweepFallback",       kScript, SweepFallback);
    a_vm->RegisterFunction("GetNextMorphActor",   kScript, GetNextMorphActor);
    a_vm->RegisterFunction("ApplyBody",           kScript, ApplyBody);
    a_vm->RegisterFunction("SetActorExcluded",    kScript, SetActorExcluded);
    a_vm->RegisterFunction("GetExcludeKey",       kScript, GetExcludeKey);
    a_vm->RegisterFunction("SetExcludeKey",       kScript, SetExcludeKey);
    a_vm->RegisterFunction("GetExportKey",        kScript, GetExportKey);
    a_vm->RegisterFunction("SetExportKey",        kScript, SetExportKey);
    a_vm->RegisterFunction("HasMorphsPending",    kScript, HasMorphsPending);
    a_vm->RegisterFunction("GetBodyMode",         kScript, GetBodyMode);
    a_vm->RegisterFunction("SetBodyMode",         kScript, SetBodyMode);
    a_vm->RegisterFunction("GetVolumeMorph",      kScript, GetVolumeMorph);
    a_vm->RegisterFunction("GetMorphScale",       kScript, GetMorphScale);
    a_vm->RegisterFunction("SetMorphScale",       kScript, SetMorphScale);
    a_vm->RegisterFunction("NormalizeNeckColor",  kScript, NormalizeNeckColor);
    a_vm->RegisterFunction("GetNeckColorFix",     kScript, GetNeckColorFix);
    a_vm->RegisterFunction("SetNeckColorFix",     kScript, SetNeckColorFix);
    a_vm->RegisterFunction("GetPresetOrient",     kScript, GetPresetOrient);
    a_vm->RegisterFunction("SetPresetOrient",     kScript, SetPresetOrient);
    a_vm->RegisterFunction("GetFantasyRatio",     kScript, GetFantasyRatio);
    a_vm->RegisterFunction("SetFantasyRatio",     kScript, SetFantasyRatio);
    a_vm->RegisterFunction("GetUnusualRatio",     kScript, GetUnusualRatio);
    a_vm->RegisterFunction("SetUnusualRatio",     kScript, SetUnusualRatio);
    a_vm->RegisterFunction("GetBreastUnusualRatio", kScript, GetBreastUnusualRatio);
    a_vm->RegisterFunction("SetBreastUnusualRatio", kScript, SetBreastUnusualRatio);
    a_vm->RegisterFunction("GetAthleticRatio",    kScript, GetAthleticRatio);
    a_vm->RegisterFunction("SetAthleticRatio",    kScript, SetAthleticRatio);
    a_vm->RegisterFunction("GetRaceCoherence",    kScript, GetRaceCoherence);
    a_vm->RegisterFunction("SetRaceCoherence",    kScript, SetRaceCoherence);
    a_vm->RegisterFunction("GetNaturalRatio",     kScript, GetNaturalRatio);
    a_vm->RegisterFunction("SetNaturalRatio",     kScript, SetNaturalRatio);
    a_vm->RegisterFunction("GetCurvyRatio",       kScript, GetCurvyRatio);
    a_vm->RegisterFunction("SetCurvyRatio",       kScript, SetCurvyRatio);
    a_vm->RegisterFunction("GetBaseBodyPref",     kScript, GetBaseBodyPref);
    a_vm->RegisterFunction("SetBaseBodyPref",     kScript, SetBaseBodyPref);
    a_vm->RegisterFunction("GetBaseBody",         kScript, GetBaseBody);
    a_vm->RegisterFunction("GetClothedRefit",     kScript, GetClothedRefit);
    a_vm->RegisterFunction("SetClothedRefit",     kScript, SetClothedRefit);
    a_vm->RegisterFunction("RefreshClothedRefit", kScript, RefreshClothedRefit);
    a_vm->RegisterFunction("GetReRollKey",        kScript, GetReRollKey);
    a_vm->RegisterFunction("SetReRollKey",        kScript, SetReRollKey);
    a_vm->RegisterFunction("GetFemaleBodies",     kScript, GetFemaleBodies);
    a_vm->RegisterFunction("SetFemaleBodies",     kScript, SetFemaleBodies);
    a_vm->RegisterFunction("GetMaleBodies",       kScript, GetMaleBodies);
    a_vm->RegisterFunction("SetMaleBodies",       kScript, SetMaleBodies);
    a_vm->RegisterFunction("GetMaleBuild",        kScript, GetMaleBuild);
    a_vm->RegisterFunction("SetMaleBuild",        kScript, SetMaleBuild);
    a_vm->RegisterFunction("GetActorIntensity",   kScript, GetActorIntensity);
    a_vm->RegisterFunction("GetPhysicsTier",      kScript, GetPhysicsTier);
    a_vm->RegisterFunction("GetPhysicsPercent",   kScript, GetPhysicsPercent);
    a_vm->RegisterFunction("GetArchetypeId",      kScript, GetArchetypeId);
    a_vm->RegisterFunction("GetArchetypeName",    kScript, GetArchetypeName);
    a_vm->RegisterFunction("GetButtShapeName",    kScript, GetButtShapeName);
    a_vm->RegisterFunction("GetBreastShapeName",  kScript, GetBreastShapeName);
    a_vm->RegisterFunction("HasCBPC",             kScript, HasCBPC);
    a_vm->RegisterFunction("GetFrameScore",       kScript, GetFrameScore);
    a_vm->RegisterFunction("GetMorphValue",       kScript, GetMorphValue);
    a_vm->RegisterFunction("GetMaleMorphValue",   kScript, GetMaleMorphValue);
    a_vm->RegisterFunction("GetMaleIntensity",    kScript, GetMaleIntensity);
    a_vm->RegisterFunction("GetMaleVolumeMorph",  kScript, GetMaleVolumeMorph);
    a_vm->RegisterFunction("GetMaleArchetypeId",  kScript, GetMaleArchetypeId);
    a_vm->RegisterFunction("GetMaleArchetypeName",kScript, GetMaleArchetypeName);
    a_vm->RegisterFunction("GetMalePhysicsPercent",kScript, GetMalePhysicsPercent);
    a_vm->RegisterFunction("RegenerateActor",     kScript, RegenerateActor);
    a_vm->RegisterFunction("GetToneScore",        kScript, GetToneScore);
    a_vm->RegisterFunction("IsVR",                kScript, IsVR);
    a_vm->RegisterFunction("GetVRLookTarget",     kScript, GetVRLookTarget);
    a_vm->RegisterFunction("MarkMorphsApplied",   kScript, MarkMorphsApplied);
    a_vm->RegisterFunction("HasMorphsApplied",    kScript, HasMorphsApplied);
    SKSE::log::info("OBW: Papyrus bindings registered");
    return true;
}

// Called from main.cpp at kDataLoaded: install the C++ hotkey sink (re-roll + exclude).
void InstallInputSink() {
    if (auto* mgr = RE::BSInputDeviceManager::GetSingleton()) {
        mgr->AddEventSink(KeyInputSink::GetSingleton());
        SKSE::log::info("OBW: C++ key input sink installed (re-roll/exclude hotkeys)");
    } else {
        SKSE::log::error("OBW: BSInputDeviceManager unavailable — hotkeys NOT installed");
    }
}

}  // namespace OBW::PapyrusBindings
