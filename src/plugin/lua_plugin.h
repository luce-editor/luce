#pragma once
// ============================================================================
// LuaPlugin — Loads and manages a single Lua script plugin.
//
// Each plugin gets its own isolated lua_State so that one misbehaving script
// cannot crash or corrupt other plugins.  The Lua environment exposes a
// 'luce' table with the full editor API.
// ============================================================================

#include <string>
#include <vector>
#include <functional>

// Forward-declare to avoid including Lua headers in this header.
struct lua_State;

namespace luce {

class App;

/// Metadata provided by a Lua plugin (read from the luce.plugin table).
struct LuaPluginInfo {
    std::string name        = "Unnamed Plugin";
    std::string version     = "1.0.0";
    std::string author      = "Unknown";
    std::string description = "";
};

class LuaPlugin {
public:
    LuaPlugin() = default;
    ~LuaPlugin();

    // Non-copyable
    LuaPlugin(const LuaPlugin&) = delete;
    LuaPlugin& operator=(const LuaPlugin&) = delete;
    LuaPlugin(LuaPlugin&&) = delete;

    /// Load and execute a .lua file.  Returns false on error.
    /// 'app' is passed to all API callbacks so they can reach the editor.
    bool Load(const std::string& path, App* app);

    /// Call the optional luce.on_tick(dt) function inside the script.
    void Tick(float delta_time);

    /// Call luce.on_shutdown() if defined, then close the Lua VM.
    void Shutdown();

    const LuaPluginInfo& GetInfo() const { return info_; }
    const std::string&   GetPath() const { return path_; }
    bool                 IsLoaded() const { return L_ != nullptr; }

private:
    /// Register the 'luce' table of editor API functions into L_.
    void RegisterLuceAPI(App* app);

    lua_State*   L_    = nullptr;
    LuaPluginInfo info_;
    std::string   path_;
    bool          has_tick_     = false;
    bool          has_shutdown_ = false;
};

}  // namespace luce
