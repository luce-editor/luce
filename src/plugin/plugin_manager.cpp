// ============================================================================
// PluginManager — Implementation.
// ============================================================================

#include "plugin/plugin_manager.h"
#include "ui/app.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace luce {

PluginManager::PluginManager(App* app)
    : app_(app) {}

PluginManager::~PluginManager() {
    UnloadAll();
}

void PluginManager::Init(const std::string& plugins_dir) {
    plugins_dir_ = plugins_dir;

    std::error_code ec;
    if (!fs::exists(plugins_dir, ec)) {
        fs::create_directories(plugins_dir, ec);
        return;
    }

    for (const auto& entry : fs::directory_iterator(plugins_dir, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            LoadPlugin(entry.path().string());
        } else if (entry.is_directory()) {
            // Check for plugin package: plugins/<name>/init.lua
            auto init_file = entry.path() / "init.lua";
            if (fs::exists(init_file)) {
                LoadPlugin(init_file.string());
            }
        }
    }
}

bool PluginManager::LoadPlugin(const std::string& path) {
    auto plugin = std::make_unique<LuaPlugin>();
    if (!plugin->Load(path, app_)) {
        return false;
    }
    plugins_.push_back(std::move(plugin));
    return true;
}

void PluginManager::UnloadAll() {
    for (auto& p : plugins_) {
        if (p) p->Shutdown();
    }
    plugins_.clear();
}

void PluginManager::ReloadPlugins() {
    if (app_) {
        app_->GetCommandPalette().UnregisterCommandsWithPrefix("plugin.");
    }
    UnloadAll();
    if (!plugins_dir_.empty()) {
        std::error_code ec;
        if (!fs::exists(plugins_dir_, ec)) {
            fs::create_directories(plugins_dir_, ec);
            return;
        }
        for (const auto& entry : fs::directory_iterator(plugins_dir_, ec)) {
            if (ec) break;
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                LoadPlugin(entry.path().string());
            } else if (entry.is_directory()) {
                auto init_file = entry.path() / "init.lua";
                if (fs::exists(init_file)) {
                    LoadPlugin(init_file.string());
                }
            }
        }
    }
}

bool PluginManager::UninstallPlugin(size_t index, bool delete_file) {
    if (index >= plugins_.size()) return false;

    auto& p = plugins_[index];
    std::string file_path = p->GetPath();

    p->Shutdown();
    plugins_.erase(plugins_.begin() + static_cast<std::ptrdiff_t>(index));

    if (delete_file) {
        std::error_code ec;
        if (!fs::remove(file_path, ec)) {
            std::cerr << "[PluginManager] Could not delete '" << file_path
                      << "': " << ec.message() << "\n";
            return false;
        }
    }
    return true;
}

void PluginManager::SetPluginEnabled(size_t index, bool enabled) {
    if (index < plugins_.size() && plugins_[index]) {
        plugins_[index]->SetEnabled(enabled);
    }
}

void PluginManager::Tick(float delta_time) {
    for (auto& p : plugins_) {
        if (p && p->IsEnabled()) p->Tick(delta_time);
    }
}

void PluginManager::OnFileOpened(const std::string& path) {
    for (auto& p : plugins_) {
        if (p && p->IsEnabled()) p->DispatchEvent("file_opened", path);
    }
}

void PluginManager::OnBeforeSave(const std::string& path) {
    for (auto& p : plugins_) {
        if (p && p->IsEnabled()) p->DispatchEvent("before_save", path);
    }
}

void PluginManager::OnAfterSave(const std::string& path) {
    for (auto& p : plugins_) {
        if (p && p->IsEnabled()) p->DispatchEvent("after_save", path);
    }
}

void PluginManager::OnCursorMoved(int line, int col) {
    for (auto& p : plugins_) {
        if (p && p->IsEnabled()) p->DispatchEvent("cursor_moved", line, col);
    }
}

void PluginManager::OnTextChanged(int line, int count) {
    for (auto& p : plugins_) {
        if (p && p->IsEnabled()) p->DispatchEvent("text_changed", line, count);
    }
}

std::vector<std::string> PluginManager::GetCompletions(const std::string& ext, const std::string& prefix, int line, int col) {
    std::vector<std::string> all_completions;
    for (auto& p : plugins_) {
        if (p && p->IsEnabled()) {
            auto items = p->GetCompletions(ext, prefix, line, col);
            all_completions.insert(all_completions.end(), items.begin(), items.end());
        }
    }
    return all_completions;
}

}  // namespace luce
