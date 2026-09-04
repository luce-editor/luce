// ============================================================================
// LuaPlugin — Implementation.
//
// Opens an isolated Lua VM per plugin, registers the 'luce' API table,
// then executes the script file.  After that, the plugin may register
// commands and opt into per-frame callbacks via luce.on_tick.
// ============================================================================

#include "plugin/lua_plugin.h"
#include "ui/app.h"
#include "ui/tab_bar.h"
#include "ui/command_palette.h"
#include "editor/editor_view.h"
#include "editor/text_buffer.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace luce {

// ── Helpers ───────────────────────────────────────────────────────────────

/// Retrieve the App* stored in the Lua registry.
static App* GetApp(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_luce_app");
    auto* app = static_cast<App*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return app;
}

/// Helper: get the active EditorView from the app (may be nullptr).
static EditorView* GetActiveEditor(App* app) {
    if (!app) return nullptr;
    return app->GetTabBar().ActiveEditor();
}

// ── luce.* API functions exposed to Lua ──────────────────────────────────

/// luce.register_command(id, display_name, callback_fn)
static int l_register_command(lua_State* L) {
    const char* id   = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // Store the Lua callback in the registry keyed by "cmd_<id>".
    std::string reg_key = std::string("_luce_cmd_") + id;
    lua_pushvalue(L, 3);
    lua_setfield(L, LUA_REGISTRYINDEX, reg_key.c_str());

    App* app = GetApp(L);
    if (!app) return 0;

    // Capture L and reg_key by value so the lambda is safe after this returns.
    std::string captured_key = reg_key;
    lua_State*  captured_L   = L;
    app->GetCommandPalette().RegisterCommand({
        std::string("plugin.") + id,
        std::string(name),
        "",
        [captured_L, captured_key, app]() {
            lua_getfield(captured_L, LUA_REGISTRYINDEX, captured_key.c_str());
            if (lua_isfunction(captured_L, -1)) {
                if (lua_pcall(captured_L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(captured_L, -1);
                    std::cerr << "[Lua Plugin Error] " << (err ? err : "unknown") << "\n";
                    if (app && err) {
                        app->GetToastManager().ShowError(std::string("Lua Error: ") + err);
                    }
                    lua_pop(captured_L, 1);
                }
            } else {
                lua_pop(captured_L, 1);
            }
        }
    });
    return 0;
}

/// luce.insert_text(text)
static int l_insert_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        if (auto* buf = ed->GetBuffer()) {
            auto pos = ed->GetCursors().Primary().position;
            buf->InsertText(pos.line, pos.column, text);
        }
    }
    return 0;
}

/// luce.delete_selection()
static int l_delete_selection(lua_State* L) {
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        auto& cur = ed->GetCursors().Primary();
        if (cur.HasSelection()) {
            auto start = cur.SelectionBegin();
            auto end   = cur.SelectionEnd();
            if (auto* buf = ed->GetBuffer()) {
                buf->DeleteRange(start.line, start.column, end.line, end.column);
            }
            cur.MoveTo(start);
        }
    }
    return 0;
}

/// luce.get_selection() → string
static int l_get_selection(lua_State* L) {
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        auto& cur = ed->GetCursors().Primary();
        if (cur.HasSelection()) {
            auto start = cur.SelectionBegin();
            auto end   = cur.SelectionEnd();
            if (auto* buf = ed->GetBuffer()) {
                std::string result;
                for (int line = start.line; line <= end.line && line < buf->GetLineCount(); ++line) {
                    const auto& ls = buf->GetLine(line);
                    int cs = (line == start.line) ? start.column : 0;
                    int ce = (line == end.line)   ? end.column   : static_cast<int>(ls.size());
                    if (cs < static_cast<int>(ls.size())) {
                        int len = std::max(0, std::min(ce - cs, (int)ls.size() - cs));
                        result.append(ls.data() + cs, len);
                    }
                    if (line < end.line) result += '\n';
                }
                lua_pushstring(L, result.c_str());
                return 1;
            }
        }
    }
    lua_pushstring(L, "");
    return 1;
}

/// luce.get_line(n) → string  (0-indexed)
static int l_get_line(lua_State* L) {
    int line = static_cast<int>(luaL_checkinteger(L, 1));
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        if (auto* buf = ed->GetBuffer()) {
            if (line >= 0 && line < buf->GetLineCount()) {
                lua_pushstring(L, buf->GetLine(line).c_str());
                return 1;
            }
        }
    }
    lua_pushstring(L, "");
    return 1;
}

/// luce.get_cursor_line() → int (0-indexed)
static int l_get_cursor_line(lua_State* L) {
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        lua_pushinteger(L, ed->GetCursors().Primary().position.line);
        return 1;
    }
    lua_pushinteger(L, 0);
    return 1;
}

/// luce.get_cursor_column() → int (0-indexed)
static int l_get_cursor_col(lua_State* L) {
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        lua_pushinteger(L, ed->GetCursors().Primary().position.column);
        return 1;
    }
    lua_pushinteger(L, 0);
    return 1;
}

/// luce.set_cursor(line, col)
static int l_set_cursor(lua_State* L) {
    int line = static_cast<int>(luaL_checkinteger(L, 1));
    int col  = static_cast<int>(luaL_checkinteger(L, 2));
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        ed->GetCursors().Primary().MoveTo({line, col});
    }
    return 0;
}

/// luce.get_file_path() → string
static int l_get_file_path(lua_State* L) {
    App* app = GetApp(L);
    if (auto* tab = app ? app->GetTabBar().ActiveTab() : nullptr) {
        lua_pushstring(L, tab->filepath.c_str());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

/// luce.get_file_name() → string
static int l_get_file_name(lua_State* L) {
    App* app = GetApp(L);
    if (auto* tab = app ? app->GetTabBar().ActiveTab() : nullptr) {
        if (!tab->filepath.empty()) {
            std::string name = fs::path(tab->filepath).filename().string();
            lua_pushstring(L, name.c_str());
            return 1;
        }
        lua_pushstring(L, tab->title.c_str());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

/// luce.get_file_extension() → string
static int l_get_file_extension(lua_State* L) {
    App* app = GetApp(L);
    if (auto* tab = app ? app->GetTabBar().ActiveTab() : nullptr) {
        if (!tab->filepath.empty()) {
            std::string ext = fs::path(tab->filepath).extension().string();
            lua_pushstring(L, ext.c_str());
            return 1;
        }
    }
    lua_pushstring(L, "");
    return 1;
}

/// luce.set_status(text)
static int l_set_status(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    App* app = GetApp(L);
    if (app) {
        app->GetToastManager().ShowInfo(msg);
    }
    std::cout << "[Lua Plugin Status] " << msg << "\n";
    return 0;
}

/// luce.show_notification(text, [level], [duration])
static int l_show_notification(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    const char* level_str = luaL_optstring(L, 2, "info");
    float duration = static_cast<float>(luaL_optnumber(L, 3, 5.0));

    ToastType type = ToastType::Info;
    std::string lvl = level_str;
    if (lvl == "error") type = ToastType::Error;
    else if (lvl == "warn" || lvl == "warning") type = ToastType::Warning;
    else if (lvl == "success") type = ToastType::Success;

    App* app = GetApp(L);
    if (app) {
        app->GetToastManager().Show(msg, type, duration);
    }
    return 0;
}

/// luce.show_error(text, [duration])
static int l_show_error(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    float duration = static_cast<float>(luaL_optnumber(L, 2, 7.0));
    App* app = GetApp(L);
    if (app) {
        app->GetToastManager().ShowError(msg, duration);
    }
    return 0;
}

/// luce.show_warning(text, [duration])
static int l_show_warning(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    float duration = static_cast<float>(luaL_optnumber(L, 2, 6.0));
    App* app = GetApp(L);
    if (app) {
        app->GetToastManager().ShowWarning(msg, duration);
    }
    return 0;
}

/// luce.show_info(text, [duration])
static int l_show_info(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    float duration = static_cast<float>(luaL_optnumber(L, 2, 5.0));
    App* app = GetApp(L);
    if (app) {
        app->GetToastManager().ShowInfo(msg, duration);
    }
    return 0;
}

/// luce.log(text)
static int l_log(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    std::cout << "[Lua Plugin INFO] " << msg << "\n";
    return 0;
}

/// luce.warn(text)
static int l_warn(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    std::cout << "[Lua Plugin WARN] " << msg << "\n";
    return 0;
}

// ── LuaPlugin implementation ──────────────────────────────────────────────

LuaPlugin::~LuaPlugin() {
    Shutdown();
}

void LuaPlugin::RegisterLuceAPI(App* app) {
    // Store the App* in the Lua registry for retrieval in API callbacks.
    lua_pushlightuserdata(L_, static_cast<void*>(app));
    lua_setfield(L_, LUA_REGISTRYINDEX, "_luce_app");

    static const luaL_Reg luce_funcs[] = {
        { "register_command", l_register_command   },
        { "insert_text",      l_insert_text        },
        { "delete_selection", l_delete_selection   },
        { "get_selection",    l_get_selection      },
        { "get_line",         l_get_line           },
        { "get_cursor_line",  l_get_cursor_line    },
        { "get_cursor_column",l_get_cursor_col     },
        { "set_cursor",       l_set_cursor         },
        { "get_file_path",    l_get_file_path      },
        { "get_file_name",    l_get_file_name      },
        { "get_file_extension",l_get_file_extension},
        { "set_status",       l_set_status         },
        { "show_notification",l_show_notification },
        { "show_error",       l_show_error         },
        { "show_warning",     l_show_warning       },
        { "show_info",        l_show_info          },
        { "log",              l_log                },
        { "warn",             l_warn               },
        { nullptr,            nullptr              }
    };

    luaL_newlib(L_, luce_funcs);
    lua_setglobal(L_, "luce");
}

bool LuaPlugin::Load(const std::string& path, App* app) {
    path_ = path;

    L_ = luaL_newstate();
    if (!L_) return false;

    luaL_openlibs(L_);
    RegisterLuceAPI(app);

    // Execute the script.
    if (luaL_dofile(L_, path.c_str()) != LUA_OK) {
        const char* err = lua_tostring(L_, -1);
        std::cerr << "[Lua Plugin] Failed to load '" << path << "': "
                  << (err ? err : "unknown error") << "\n";
        lua_close(L_);
        L_ = nullptr;
        return false;
    }

    // Read optional luce.plugin metadata table.
    lua_getglobal(L_, "luce");
    if (lua_istable(L_, -1)) {
        lua_getfield(L_, -1, "plugin");
        if (lua_istable(L_, -1)) {
            auto read_str = [&](const char* key, std::string& out) {
                lua_getfield(L_, -1, key);
                if (lua_isstring(L_, -1)) out = lua_tostring(L_, -1);
                lua_pop(L_, 1);
            };
            read_str("name",        info_.name);
            read_str("version",     info_.version);
            read_str("author",      info_.author);
            read_str("description", info_.description);
        }
        lua_pop(L_, 1); // pop luce.plugin
    }
    lua_pop(L_, 1); // pop luce

    // Detect optional lifecycle callbacks at the top level.
    lua_getglobal(L_, "on_tick");
    has_tick_ = lua_isfunction(L_, -1);
    lua_pop(L_, 1);

    lua_getglobal(L_, "on_shutdown");
    has_shutdown_ = lua_isfunction(L_, -1);
    lua_pop(L_, 1);

    std::string script_name = fs::path(path).filename().string();
    std::cout << "[Lua Plugin] Loaded '" << info_.name
              << "' v" << info_.version
              << " (" << script_name << ")\n";
    return true;
}

void LuaPlugin::Tick(float delta_time) {
    if (!L_ || !has_tick_) return;
    lua_getglobal(L_, "on_tick");
    if (lua_isfunction(L_, -1)) {
        lua_pushnumber(L_, static_cast<lua_Number>(delta_time));
        if (lua_pcall(L_, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L_, -1);
            std::cerr << "[Lua Plugin Tick Error] " << (err ? err : "?") << "\n";
            lua_pop(L_, 1);
            has_tick_ = false; // Disable further ticks on error.
        }
    } else {
        lua_pop(L_, 1);
    }
}

void LuaPlugin::Shutdown() {
    if (!L_) return;
    if (has_shutdown_) {
        lua_getglobal(L_, "on_shutdown");
        if (lua_isfunction(L_, -1)) {
            if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
                lua_pop(L_, 1);
            }
        } else {
            lua_pop(L_, 1);
        }
    }
    lua_close(L_);
    L_ = nullptr;
}

}  // namespace luce
