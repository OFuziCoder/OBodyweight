#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "WeightManager.hpp"
#include "Config.hpp"

namespace OBW::PapyrusBindings { bool Register(RE::BSScript::IVirtualMachine*); }

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    // Log
    auto path = SKSE::log::log_directory();
    if (path) {
        *path /= "OBodyNGWeight.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto logger = std::make_shared<spdlog::logger>("OBW", std::move(sink));
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::debug);
        spdlog::set_default_logger(std::move(logger));
    }

    // Read INI defaults (FOMOD-installed preset) before WeightManager is constructed.
    OBW::Config::Load();

    // Papyrus
    auto* papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus) return false;
    papyrus->Register(OBW::PapyrusBindings::Register);

    // Cosave (UID unique across all SKSE plugins)
    auto* serial = SKSE::GetSerializationInterface();
    if (serial) {
        serial->SetUniqueID(OBW::WeightManager::kRecordUID);
        serial->SetSaveCallback(OBW::WeightManager::SaveCallback);
        serial->SetLoadCallback(OBW::WeightManager::LoadCallback);
        serial->SetRevertCallback(OBW::WeightManager::RevertCallback);
    }

    // NOTE: OBW no longer changes the actor's real weight (base->weight). Changing it
    // caused neck seams (the baked head facegen is at the editor weight and can't follow)
    // and outfit/body weight mismatches. Body size now comes purely from morphs. The
    // per-NPC weight value is kept as a "mock weight" that drives those morphs.

    SKSE::log::info("OBodyNGWeight loaded");
    return true;
}
