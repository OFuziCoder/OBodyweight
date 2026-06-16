#include "WeightManager.hpp"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>   // GetModuleHandleA (CBPC detection)

namespace OBW::PapyrusBindings {

namespace {

constexpr std::string_view kScript{ "OBW_Native" };

// Generate and return the weight for the actor (without applying it).
float GetWeight(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    return WeightManager::GetSingleton().GenerateWeight(a_actor);
}

// Mode: 0=Random, 1=Seeded/Deterministic, 2=NpcDefault
std::int32_t GetMode(RE::StaticFunctionTag*) {
    return static_cast<std::int32_t>(WeightManager::GetSingleton().GetMode());
}

void SetMode(RE::StaticFunctionTag*, std::int32_t a_mode) {
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

std::int32_t GetReRollKey(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().GetReRollKey();
}

void SetReRollKey(RE::StaticFunctionTag*, std::int32_t a_key) {
    WeightManager::GetSingleton().SetReRollKey(a_key);
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

// CBPC ships cbp.dll — if it's loaded, the physics integration is safe to use.
bool HasCBPC(RE::StaticFunctionTag*) {
    return GetModuleHandleA("cbp.dll") != nullptr;
}

std::int32_t ReprocessAllLoaded(RE::StaticFunctionTag*) {
    return WeightManager::GetSingleton().ReprocessAllLoaded();
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

void MarkMorphsApplied(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    if (a_actor) WeightManager::GetSingleton().MarkMorphsApplied(a_actor->GetFormID());
}

bool HasMorphsApplied(RE::StaticFunctionTag*, RE::Actor* a_actor) {
    if (!a_actor) return false;
    return WeightManager::GetSingleton().HasMorphsApplied(a_actor->GetFormID());
}

}  // namespace

bool Register(RE::BSScript::IVirtualMachine* a_vm) {
    a_vm->RegisterFunction("GetWeight",           kScript, GetWeight);
    a_vm->RegisterFunction("GetMode",             kScript, GetMode);
    a_vm->RegisterFunction("SetMode",             kScript, SetMode);
    a_vm->RegisterFunction("GetBias",             kScript, GetBias);
    a_vm->RegisterFunction("SetBias",             kScript, SetBias);
    a_vm->RegisterFunction("GetSeed",             kScript, GetSeed);
    a_vm->RegisterFunction("RegenerateSeed",      kScript, RegenerateSeed);
    a_vm->RegisterFunction("QueueForMorphs",      kScript, QueueForMorphs);
    a_vm->RegisterFunction("ReprocessAllLoaded",  kScript, ReprocessAllLoaded);
    a_vm->RegisterFunction("GetNextMorphActor",   kScript, GetNextMorphActor);
    a_vm->RegisterFunction("HasMorphsPending",    kScript, HasMorphsPending);
    a_vm->RegisterFunction("GetBodyMode",         kScript, GetBodyMode);
    a_vm->RegisterFunction("SetBodyMode",         kScript, SetBodyMode);
    a_vm->RegisterFunction("GetVolumeMorph",      kScript, GetVolumeMorph);
    a_vm->RegisterFunction("GetMorphScale",       kScript, GetMorphScale);
    a_vm->RegisterFunction("SetMorphScale",       kScript, SetMorphScale);
    a_vm->RegisterFunction("GetFantasyRatio",     kScript, GetFantasyRatio);
    a_vm->RegisterFunction("SetFantasyRatio",     kScript, SetFantasyRatio);
    a_vm->RegisterFunction("GetUnusualRatio",     kScript, GetUnusualRatio);
    a_vm->RegisterFunction("SetUnusualRatio",     kScript, SetUnusualRatio);
    a_vm->RegisterFunction("GetBreastUnusualRatio", kScript, GetBreastUnusualRatio);
    a_vm->RegisterFunction("SetBreastUnusualRatio", kScript, SetBreastUnusualRatio);
    a_vm->RegisterFunction("GetAthleticRatio",    kScript, GetAthleticRatio);
    a_vm->RegisterFunction("SetAthleticRatio",    kScript, SetAthleticRatio);
    a_vm->RegisterFunction("GetReRollKey",        kScript, GetReRollKey);
    a_vm->RegisterFunction("SetReRollKey",        kScript, SetReRollKey);
    a_vm->RegisterFunction("GetMaleBodies",       kScript, GetMaleBodies);
    a_vm->RegisterFunction("SetMaleBodies",       kScript, SetMaleBodies);
    a_vm->RegisterFunction("GetMaleBuild",        kScript, GetMaleBuild);
    a_vm->RegisterFunction("SetMaleBuild",        kScript, SetMaleBuild);
    a_vm->RegisterFunction("GetActorIntensity",   kScript, GetActorIntensity);
    a_vm->RegisterFunction("GetPhysicsTier",      kScript, GetPhysicsTier);
    a_vm->RegisterFunction("GetPhysicsPercent",   kScript, GetPhysicsPercent);
    a_vm->RegisterFunction("GetArchetypeId",      kScript, GetArchetypeId);
    a_vm->RegisterFunction("GetArchetypeName",    kScript, GetArchetypeName);
    a_vm->RegisterFunction("HasCBPC",             kScript, HasCBPC);
    a_vm->RegisterFunction("GetFrameScore",       kScript, GetFrameScore);
    a_vm->RegisterFunction("GetMorphValue",       kScript, GetMorphValue);
    a_vm->RegisterFunction("GetMaleMorphValue",   kScript, GetMaleMorphValue);
    a_vm->RegisterFunction("GetMaleIntensity",    kScript, GetMaleIntensity);
    a_vm->RegisterFunction("RegenerateActor",     kScript, RegenerateActor);
    a_vm->RegisterFunction("GetToneScore",        kScript, GetToneScore);
    a_vm->RegisterFunction("IsVR",                kScript, IsVR);
    a_vm->RegisterFunction("GetVRLookTarget",     kScript, GetVRLookTarget);
    a_vm->RegisterFunction("MarkMorphsApplied",   kScript, MarkMorphsApplied);
    a_vm->RegisterFunction("HasMorphsApplied",    kScript, HasMorphsApplied);
    SKSE::log::info("OBW: Papyrus bindings registered");
    return true;
}

}  // namespace OBW::PapyrusBindings
