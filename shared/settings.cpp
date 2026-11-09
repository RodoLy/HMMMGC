// See settings.h.

#include "settings.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace settings {

Settings Settings::load(const std::filesystem::path& file) {
    Settings settings;
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return settings;  // no file yet: every value is its default
    }
    // An unreadable file starts from the defaults and is replaced on saving.
    nlohmann::json document = nlohmann::json::parse(stream, nullptr, /*allow_exceptions=*/false);
    if (document.is_object()) {
        settings.document_ = std::move(document);
    }
    return settings;
}

bool Settings::save(const std::filesystem::path& file) const {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    // Written beside it and then moved over it, so that a failed write never leaves half a file.
    const std::filesystem::path partial = file.parent_path() / (file.filename().string() + ".new");
    {
        std::ofstream stream(partial, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return false;
        }
        stream << document_.dump(2) << "\n";
        if (!stream) {
            return false;
        }
    }
    std::filesystem::rename(partial, file, error);
    return !error;
}

int Settings::number(const char* name, int fallback, int minimum, int maximum) const {
    const auto found = document_.find(name);
    if (found == document_.end() || !found->is_number_integer()) {
        return fallback;
    }
    const int value = found->get<int>();
    return value >= minimum && value <= maximum ? value : fallback;
}

int Settings::one_of(const char* name, std::span<const int> allowed) const {
    const auto found = document_.find(name);
    if (found != document_.end() && found->is_number_integer() &&
        std::ranges::find(allowed, found->get<int>()) != allowed.end()) {
        return found->get<int>();
    }
    return allowed.front();
}

bool Settings::flag(const char* name, bool fallback) const {
    const auto found = document_.find(name);
    return found != document_.end() && found->is_boolean() ? found->get<bool>() : fallback;
}

std::string Settings::text(const char* name, const std::string& fallback) const {
    const auto found = document_.find(name);
    return found != document_.end() && found->is_string() ? found->get<std::string>() : fallback;
}

void Settings::set(const char* name, const nlohmann::json& value) { document_[name] = value; }

bool Settings::mod_enabled(const std::string& id) const {
    const auto mods = document_.find(MODS);
    if (mods == document_.end() || !mods->is_object()) {
        return true;
    }
    const auto found = mods->find(id);
    return found == mods->end() || !found->is_boolean() || found->get<bool>();
}

void Settings::set_mod_enabled(const std::string& id, bool enabled) {
    if (!document_.contains(MODS) || !document_[MODS].is_object()) {
        document_[MODS] = nlohmann::json::object();
    }
    document_[MODS][id] = enabled;
}

}  // namespace settings
