// See mods.h.

#include "mods.h"

#include "texts.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace mods {
namespace {

// A text of the manifest in the launcher's language, or in English, or empty.
std::string translated(const nlohmann::json& manifest, const char* name) {
    const auto found = manifest.find(name);
    if (found == manifest.end() || !found->is_object()) {
        return {};
    }
    for (const char* code : {texts::language_code(), "en"}) {
        const auto text = found->find(code);
        if (text != found->end() && text->is_string()) {
            return text->get<std::string>();
        }
    }
    return {};
}

std::string text_of(const nlohmann::json& manifest, const char* name) {
    const auto found = manifest.find(name);
    return found != manifest.end() && found->is_string() ? found->get<std::string>() : std::string();
}

}  // namespace

std::vector<Mod> load(const std::filesystem::path& folder, const std::vector<std::string>& ids) {
    std::vector<Mod> loaded;
    for (const std::string& id : ids) {
        Mod mod;
        mod.id = id;
        mod.name = id;
        std::ifstream stream(folder / id / "mod.json", std::ios::binary);
        const nlohmann::json manifest = nlohmann::json::parse(stream, nullptr, /*allow_exceptions=*/false);
        if (manifest.is_object()) {
            mod.version = text_of(manifest, "version");
            if (const auto authors = manifest.find("authors"); authors != manifest.end() && authors->is_array()) {
                for (const auto& author : *authors) {
                    if (author.is_string()) {
                        mod.authors += (mod.authors.empty() ? "" : ", ") + author.get<std::string>();
                    }
                }
            }
            if (std::string name = translated(manifest, "name"); !name.empty()) {
                mod.name = std::move(name);
            }
            mod.summary = translated(manifest, "summary");
            mod.description = translated(manifest, "description");
            mod.warning = translated(manifest, "warning");
            if (const auto settings = manifest.find("settings"); settings != manifest.end() && settings->is_array()) {
                for (const auto& entry : *settings) {
                    if (!entry.is_object() || entry.value("type", "") != "percent") {
                        continue;
                    }
                    Setting setting;
                    setting.key = entry.value("key", "");
                    setting.environment = entry.value("environment", "");
                    setting.minimum = entry.value("minimum", 0);
                    setting.maximum = entry.value("maximum", 100);
                    setting.fallback = entry.value("default", setting.maximum);
                    setting.step = entry.value("step", 5);
                    setting.label = translated(entry, "label");
                    if (!setting.key.empty() && setting.minimum < setting.maximum) {
                        mod.settings.push_back(std::move(setting));
                    }
                }
            }
        }
        loaded.push_back(std::move(mod));
    }
    return loaded;
}

}  // namespace mods
