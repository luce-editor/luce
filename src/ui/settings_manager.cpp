// ============================================================================
// SettingsManager — Implementation.
// ============================================================================

#include "settings_manager.h"
#include "platform.h"
#include "external/json.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace luce {

SettingsManager::SettingsManager() {
    // Attempt to load settings from default location
    EnsureDefaultSettingsFile();
    LoadFromFile();
}

std::string SettingsManager::GetSettingsPath() const {
    return platform::GetExecutableDir() + "/settings.json";
}

bool SettingsManager::EnsureDefaultSettingsFile() {
    std::string path = GetSettingsPath();
    if (fs::exists(path)) return true;

    return SaveToFile(path);
}

bool SettingsManager::LoadFromFile(const std::string& custom_path) {
    std::string path = custom_path.empty() ? GetSettingsPath() : custom_path;
    if (!fs::exists(path)) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        // Read file content
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

        // nlohmann::json parse with comments allowed
        json j = json::parse(content, nullptr, true, true);

        // Editor settings (nested under "editor" or at root for flexibility)
        const json* ed = nullptr;
        if (j.contains("editor") && j["editor"].is_object()) {
            ed = &j["editor"];
        }

        auto get_int = [&](const char* key, int default_val) -> int {
            if (ed && ed->contains(key) && (*ed)[key].is_number_integer()) {
                return (*ed)[key].get<int>();
            }
            if (j.contains(key) && j[key].is_number_integer()) {
                return j[key].get<int>();
            }
            return default_val;
        };

        auto get_bool = [&](const char* key, bool default_val) -> bool {
            if (ed && ed->contains(key) && (*ed)[key].is_boolean()) {
                return (*ed)[key].get<bool>();
            }
            if (j.contains(key) && j[key].is_boolean()) {
                return j[key].get<bool>();
            }
            return default_val;
        };

        settings_.editor_font_size = std::clamp(get_int("font_size", settings_.editor_font_size), 8, 48);
        settings_.tab_size = std::clamp(get_int("tab_size", settings_.tab_size), 1, 16);
        settings_.use_spaces = get_bool("use_spaces", settings_.use_spaces);
        settings_.show_minimap = get_bool("show_minimap", settings_.show_minimap);
        settings_.show_line_numbers = get_bool("show_line_numbers", settings_.show_line_numbers);
        settings_.highlight_current_line = get_bool("highlight_current_line", settings_.highlight_current_line);
        settings_.zoom_with_mouse_wheel = get_bool("zoom_with_mouse_wheel", settings_.zoom_with_mouse_wheel);
        settings_.cursor_blinking = get_bool("cursor_blinking", settings_.cursor_blinking);

        // UI & Appearance settings (nested under "ui" or at root)
        const json* ui = nullptr;
        if (j.contains("ui") && j["ui"].is_object()) {
            ui = &j["ui"];
        }

        if (ui && ui->contains("font_size") && (*ui)["font_size"].is_number_integer()) {
            settings_.ui_font_size = std::clamp((*ui)["font_size"].get<int>(), 10, 28);
        } else if (j.contains("ui_font_size") && j["ui_font_size"].is_number_integer()) {
            settings_.ui_font_size = std::clamp(j["ui_font_size"].get<int>(), 10, 28);
        }

        if (ui && ui->contains("scale") && (*ui)["scale"].is_number()) {
            settings_.ui_scale = std::clamp((*ui)["scale"].get<float>(), 0.5f, 2.5f);
        } else if (j.contains("scale") && j["scale"].is_number()) {
            settings_.ui_scale = std::clamp(j["scale"].get<float>(), 0.5f, 2.5f);
        }

        if (ui && ui->contains("theme") && (*ui)["theme"].is_string()) {
            settings_.theme_name = (*ui)["theme"].get<std::string>();
        } else if (j.contains("theme") && j["theme"].is_string()) {
            settings_.theme_name = j["theme"].get<std::string>();
        }

        if (ui && ui->contains("auto_save") && (*ui)["auto_save"].is_string()) {
            settings_.auto_save_mode = (*ui)["auto_save"].get<std::string>();
        } else if (j.contains("auto_save") && j["auto_save"].is_string()) {
            settings_.auto_save_mode = j["auto_save"].get<std::string>();
        }

        if (ui && ui->contains("show_welcome_on_startup") && (*ui)["show_welcome_on_startup"].is_boolean()) {
            settings_.show_welcome_on_startup = (*ui)["show_welcome_on_startup"].get<bool>();
        } else if (j.contains("show_welcome_on_startup") && j["show_welcome_on_startup"].is_boolean()) {
            settings_.show_welcome_on_startup = j["show_welcome_on_startup"].get<bool>();
        }

        if (j.contains("recent_projects") && j["recent_projects"].is_array()) {
            settings_.recent_projects.clear();
            for (const auto& item : j["recent_projects"]) {
                if (item.is_string()) {
                    std::string p = item.get<std::string>();
                    if (!p.empty() && fs::exists(p)) {
                        settings_.recent_projects.push_back(p);
                    }
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool SettingsManager::SaveToFile(const std::string& custom_path) {
    std::string path = custom_path.empty() ? GetSettingsPath() : custom_path;

    try {
        json j;
        j["$schema"] = "https://raw.githubusercontent.com/luce-editor/luce/main/schema/settings.schema.json";

        json ed;
        ed["font_size"] = settings_.editor_font_size;
        ed["tab_size"] = settings_.tab_size;
        ed["use_spaces"] = settings_.use_spaces;
        ed["show_minimap"] = settings_.show_minimap;
        ed["show_line_numbers"] = settings_.show_line_numbers;
        ed["highlight_current_line"] = settings_.highlight_current_line;
        ed["zoom_with_mouse_wheel"] = settings_.zoom_with_mouse_wheel;
        ed["cursor_blinking"] = settings_.cursor_blinking;
        j["editor"] = ed;

        json ui;
        ui["font_size"] = settings_.ui_font_size;
        ui["scale"] = settings_.ui_scale;
        ui["theme"] = settings_.theme_name;
        ui["auto_save"] = settings_.auto_save_mode;
        ui["show_welcome_on_startup"] = settings_.show_welcome_on_startup;
        j["ui"] = ui;

        j["recent_projects"] = settings_.recent_projects;

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) return false;

        file << j.dump(4) << "\n";
        return true;
    } catch (...) {
        return false;
    }
}

void SettingsManager::AddRecentProject(const std::string& path) {
    if (path.empty()) return;

    std::error_code ec;
    std::string norm_path = fs::weakly_canonical(path, ec).string();
    if (ec) norm_path = path;

    // Normalize slashes to forward slashes for clean display/JSON
    std::replace(norm_path.begin(), norm_path.end(), '\\', '/');

    auto& list = settings_.recent_projects;
    list.erase(std::remove(list.begin(), list.end(), norm_path), list.end());
    list.insert(list.begin(), norm_path);

    // Keep at most 10 recent projects
    if (list.size() > 10) {
        list.resize(10);
    }

    SaveToFile();
}

}  // namespace luce
