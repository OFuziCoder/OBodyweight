#include "PresetManager.hpp"
#include "Debug.hpp"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace OBW {

namespace {

// Tuning -----------------------------------------------------------------------------------
constexpr float  kEps  = 0.03f;   // |big-small| <= this (SKEE units) counts as a static slider
constexpr float  kLean = 0.35f;   // static volume: synthesized weight-0 end = V*(1-kLean)
constexpr size_t kMaxSliders = 128;  // Papyrus arrays cap at 128; keep names/values aligned

constexpr const char* kPresetDir = R"(.\Data\CalienteTools\BodySlide\SliderPresets)";

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string Trim(std::string_view sv) {
    size_t b = 0, e = sv.size();
    while (b < e && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(b, e - b));
}

std::string Key(std::string_view name) { return ToLower(Trim(name)); }

// Extract the value of attribute `key` from an XML start-tag string (key="value").
std::string Attr(const std::string& tag, const char* key) {
    std::string needle = std::string(key) + "=\"";
    auto p = tag.find(needle);
    if (p == std::string::npos) return {};
    p += needle.size();
    auto e = tag.find('"', p);
    if (e == std::string::npos) return {};
    return tag.substr(p, e - p);
}

float Smoothstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Decode the XML entities BodySlide may write in a preset name (e.g. "Dibella&apos;s") so the
// cache key matches the DECODED name OBody returns from GetPresetAssignedToActor.
std::string DecodeEntities(std::string s) {
    struct Ent { const char* from; char to; };
    static const Ent kEnts[] = {
        { "&apos;", '\'' }, { "&quot;", '"' }, { "&lt;", '<' }, { "&gt;", '>' }, { "&amp;", '&' },
    };
    for (const auto& e : kEnts) {
        const std::string from = e.from;
        size_t p = 0;
        while ((p = s.find(from, p)) != std::string::npos) {
            s.replace(p, from.size(), 1, e.to);
            p += 1;
        }
    }
    return s;
}

}  // namespace

PresetManager& PresetManager::GetSingleton() {
    static PresetManager instance;
    return instance;
}

bool PresetManager::IsVolumeSlider(const std::string& a_lowerName) {
    // The main "body size" sliders: when an author froze them (static), the mock weight
    // synthesizes a lean->full axis on these. Shape/detail sliders are left untouched.
    static const std::unordered_set<std::string> kVolume = {
        "breasts", "breastsnewsh", "breastsfantasy", "doublemelon",
        "butt", "bigbutt", "buttclassic", "roundass", "applecheeks",
        "hips", "hipupperwidth",
        "thighs", "thighfbthicc_v2", "thighoutsidethicc_v2",
        "calfsize", "arms", "chubbyarms", "chubbybutt", "chubbylegs",
    };
    return kVolume.contains(a_lowerName);
}

void PresetManager::ParseFileInto(const std::filesystem::path& a_path) {
  try {
    std::ifstream f(a_path, std::ios::binary);   // path overload opens unicode filenames correctly
    if (!f) return;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    size_t pos = 0;
    while (true) {
        const auto ps = data.find("<Preset", pos);
        if (ps == std::string::npos) break;
        const auto headEnd = data.find('>', ps);
        if (headEnd == std::string::npos) break;
        const std::string head = data.substr(ps, headEnd - ps + 1);
        const std::string name = DecodeEntities(Attr(head, "name"));

        const auto blockEnd = data.find("</Preset>", headEnd);
        const size_t bodyStart = headEnd + 1;
        const size_t bodyEnd   = (blockEnd == std::string::npos) ? data.size() : blockEnd;
        const std::string block = data.substr(bodyStart, bodyEnd - bodyStart);

        SliderSet set;
        std::unordered_map<std::string, size_t> idx;  // lowered slider name -> index in set
        size_t sp = 0;
        while (true) {
            const auto ss = block.find("<SetSlider", sp);
            if (ss == std::string::npos) break;
            const auto se = block.find('>', ss);
            if (se == std::string::npos) break;
            const std::string tag = block.substr(ss, se - ss + 1);
            sp = se + 1;

            const std::string sname = Attr(tag, "name");
            if (sname.empty()) continue;
            const std::string size = Attr(tag, "size");
            const std::string valStr = Attr(tag, "value");
            float v = 0.0f;
            try {
                v = std::stof(valStr) / 100.0f;  // BodySlide percent -> SKEE units
            } catch (...) {
                continue;
            }

            const std::string low = ToLower(sname);
            size_t i;
            if (auto it = idx.find(low); it != idx.end()) {
                i = it->second;
            } else {
                i = set.size();
                set.push_back(Slider{ sname, 0.0f, 0.0f });
                idx.emplace(low, i);
            }
            if (size == "big") set[i].hi = v;
            else               set[i].lo = v;  // "small" (or unlabeled) -> low-weight end
        }

        if (!name.empty()) _cache[Key(name)] = std::move(set);

        if (blockEnd == std::string::npos) break;
        pos = blockEnd + 9;  // strlen("</Preset>")
    }
  } catch (...) {
    SKSE::log::warn("PresetManager: error parsing a preset file (skipped)");
  }
}

const PresetManager::SliderSet& PresetManager::Ensure(std::string_view a_preset) {
    const std::string key = Key(a_preset);
    if (key.empty()) {
        static const SliderSet empty;
        return empty;
    }
    if (auto it = _cache.find(key); it != _cache.end()) return it->second;

    // Fast path: BodySlide saves a preset to "<name>.xml", so try that file directly.
    {
        const std::filesystem::path path = std::filesystem::path(kPresetDir) / (Trim(a_preset) + ".xml");
        ParseFileInto(path);
        if (auto it = _cache.find(key); it != _cache.end()) {
            if (g_debugLog)
                SKSE::log::info("PresetManager: '{}' = {} sliders (direct file)", key, it->second.size());
            return it->second;
        }
    }

    // Fallback: the preset name differs from its filename (or lives with others in one file).
    // Scan the whole folder once and cache every preset found.
    if (!_fullScanned) {
        _fullScanned = true;
        std::error_code ec;
        const std::filesystem::path dir(kPresetDir);
        // Manual increment with error_code: the range-for's operator++ THROWS on an iteration
        // error (a torn VFS entry, long path, etc.), which would escape into the Papyrus VM and
        // crash. increment(ec) never throws -> we just stop the scan on error.
        std::filesystem::directory_iterator it(dir, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code ec2;
            if (!it->is_regular_file(ec2)) continue;
            // Compare/read via the WIDE path (native()) — no codepage conversion -> never throws on
            // a non-ASCII filename (the .string() conversion is what blew up the scan before).
            std::wstring ext = it->path().extension().native();
            for (auto& c : ext) c = static_cast<wchar_t>(std::towlower(c));
            if (ext == L".xml") ParseFileInto(it->path());
        }
        if (g_debugLog)
            SKSE::log::info("PresetManager: scanned SliderPresets ({} preset(s) cached){}",
                            _cache.size(), ec ? " [stopped on iteration error]" : "");
        if (auto it = _cache.find(key); it != _cache.end()) {
            if (g_debugLog)
                SKSE::log::info("PresetManager: '{}' = {} sliders (after scan)", key, it->second.size());
            return it->second;
        }
    }

    // Negative cache (empty set) so a missing preset isn't retried for every actor.
    SKSE::log::warn("PresetManager: preset '{}' NOT FOUND in SliderPresets (path or name mismatch)", key);
    return _cache.emplace(key, SliderSet{}).first->second;
}

std::vector<std::string> PresetManager::GetSliderNames(std::string_view a_preset) {
    std::scoped_lock lock(_mutex);
    const SliderSet& set = Ensure(a_preset);
    std::vector<std::string> names;
    names.reserve(std::min(set.size(), kMaxSliders));
    for (const auto& s : set) {
        if (names.size() >= kMaxSliders) break;
        names.push_back(s.name);
    }
    return names;
}

std::vector<std::pair<std::string, float>> PresetManager::ComputeAll(std::string_view a_preset, float a_mockWeight) {
    std::scoped_lock lock(_mutex);
    const SliderSet& set = Ensure(a_preset);
    const float t = Smoothstep(std::clamp(a_mockWeight / 100.0f, 0.0f, 1.0f));

    std::vector<std::pair<std::string, float>> out;
    out.reserve(set.size());
    for (const auto& s : set) {
        float lo = s.lo, hi = s.hi;
        if (std::abs(hi - lo) <= kEps) {
            const float V = (hi != 0.0f) ? hi : lo;
            if (V > 0.0f && IsVolumeSlider(ToLower(s.name))) {
                lo = V * (1.0f - kLean);
                hi = V;
            } else {
                lo = hi = V;
            }
        }
        out.emplace_back(s.name, lo + (hi - lo) * t);
    }
    return out;
}

std::vector<float> PresetManager::GetMorphs(std::string_view a_preset, float a_mockWeight) {
    std::scoped_lock lock(_mutex);
    const SliderSet& set = Ensure(a_preset);
    const float t = Smoothstep(std::clamp(a_mockWeight / 100.0f, 0.0f, 1.0f));

    std::vector<float> out;
    out.reserve(std::min(set.size(), kMaxSliders));
    for (const auto& s : set) {
        if (out.size() >= kMaxSliders) break;
        float lo = s.lo, hi = s.hi;
        if (std::abs(hi - lo) <= kEps) {
            // Static slider: the author gave no weight curve.
            const float V = (hi != 0.0f) ? hi : lo;
            if (V > 0.0f && IsVolumeSlider(ToLower(s.name))) {
                lo = V * (1.0f - kLean);  // synthesized lean end (only scales DOWN -> no overshoot)
                hi = V;
            } else {
                lo = hi = V;  // shape/detail: keep constant
            }
        }
        out.push_back(lo + (hi - lo) * t);
    }
    return out;
}

}  // namespace OBW
