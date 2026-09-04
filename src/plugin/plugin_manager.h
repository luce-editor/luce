#pragma once
// ============================================================================
// PluginManager — Discovers and manages Lua script plugins.
//
// Replaces the old C ABI DLL/SO system with a Lua-based scripting model.
// Each .lua file in the plugins/ directory is loaded into an isolated
// lua_State via LuaPlugin.
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

    /// Scan the given directory and load all .lua files found.
    void Init(const std::string& plugins_dir);

    /// Load a single Lua plugin file. Returns false on error.
    bool LoadPlugin(const std::string& path);

    /// Unload all plugins and close their Lua VMs.
    void UnloadAll();

    /// Reload all plugins from the plugins directory.
    void ReloadPlugins();

    /// Unload a plugin by index (and optionally delete its file).
    bool UninstallPlugin(size_t index, bool delete_file = false);

    /// Call on_tick(dt) on every loaded plugin that registered it.
    void Tick(float delta_time);

    const std::vector<std::unique_ptr<LuaPlugin>>& GetLoadedPlugins() const {
        return plugins_;
    }

private:
    App*                                   app_         = nullptr;
    std::string                            plugins_dir_;
    std::vector<std::unique_ptr<LuaPlugin>> plugins_;
};

}  // namespace luce
