#pragma once
// ============================================================================
// PluginManager — Discovers and manages Lua script plugins.
//
// Replaces the old C ABI DLL/SO system with a Lua-based scripting model.
// Each .lua file or plugin folder in the plugins/ directory is loaded into
// an isolated lua_State via LuaPlugin.
// ============================================================================

#include "plugin/lua_plugin.h"

#include <memory>
#include <string>
#include <vector>

namespace luce {

class App;

class PluginManager {
public:
    explicit PluginManager(App* app);
    ~PluginManager();

    /// Scan the given directory and load all .lua files and plugin folders found.
    void Init(const std::string& plugins_dir);

    /// Load a single Lua plugin file or directory. Returns false on error.
    bool LoadPlugin(const std::string& path);

    /// Unload all plugins and close their Lua VMs.
    void UnloadAll();

    /// Reload all plugins from the plugins directory.
    void ReloadPlugins();

    /// Unload a plugin by index (and optionally delete its file).
    bool UninstallPlugin(size_t index, bool delete_file = false);

    /// Enable or disable a plugin by index.
    void SetPluginEnabled(size_t index, bool enabled);

    /// Call on_tick(dt) on every loaded plugin that registered it.
    void Tick(float delta_time);

    /// Dispatch event hooks to all active plugins.
    void OnFileOpened(const std::string& path);
    void OnBeforeSave(const std::string& path);
    void OnAfterSave(const std::string& path);
    void OnCursorMoved(int line, int col);
    void OnTextChanged(int line, int count);

    /// Gather completions from plugins registering a completion provider.
    std::vector<std::string> GetCompletions(const std::string& ext, const std::string& prefix, int line, int col);

    const std::vector<std::unique_ptr<LuaPlugin>>& GetLoadedPlugins() const {
        return plugins_;
    }

private:
    App*                                   app_         = nullptr;
    std::string                            plugins_dir_;
    std::vector<std::unique_ptr<LuaPlugin>> plugins_;
};

}  // namespace luce
