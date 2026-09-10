// ============================================================================
// LuaPlugin — Implementation.
//
// Opens an isolated Lua VM per plugin, registers the 'luce' API table,
// then executes the script file.  After that, the plugin may register
// commands, event hooks, diagnostic providers, and autocompletions.
// ============================================================================

#include "plugin/lua_plugin.h"
#include "ui/app.h"
#include "ui/tab_bar.h"
#include "ui/command_palette.h"
#include "ui/file_explorer.h"
#include "editor/editor_view.h"
#include "editor/text_buffer.h"
#include "editor/diagnostic.h"
#include "platform.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <filesystem>
#include <iostream>
#include <algorithm>

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

/// luce.register_command(id, display_name, [shortcut], callback_fn)
static int l_register_command(lua_State* L) {
    const char* id   = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);

    std::string shortcut = "";
    int fn_idx = 3;
    if (lua_isstring(L, 3)) {
        shortcut = lua_tostring(L, 3);
        fn_idx = 4;
    }
    luaL_checktype(L, fn_idx, LUA_TFUNCTION);

    // Store the Lua callback in the registry keyed by "_luce_cmd_<id>".
    std::string reg_key = std::string("_luce_cmd_") + id;
    lua_pushvalue(L, fn_idx);
    lua_setfield(L, LUA_REGISTRYINDEX, reg_key.c_str());

    App* app = GetApp(L);
    if (!app) return 0;

    std::string captured_key = reg_key;
    lua_State*  captured_L   = L;
    app->GetCommandPalette().RegisterCommand({
        std::string("plugin.") + id,
        std::string(name),
        shortcut,
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

/// luce.on(event_name, callback_fn)
/// Registers an event hook: "file_opened", "before_save", "after_save", "cursor_moved", "text_changed"
static int l_on(lua_State* L) {
    const char* event_name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Retrieve or create registry table "_luce_events"
    lua_getfield(L, LUA_REGISTRYINDEX, "_luce_events");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "_luce_events");
    }

    // Get table for event_name: _luce_events[event_name]
    lua_getfield(L, -1, event_name);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, event_name);
    }

    // Append callback to table
    int len = static_cast<int>(lua_rawlen(L, -1));
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, len + 1);

    lua_pop(L, 2); // pop event table and _luce_events
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

/// luce.get_text() → string
static int l_get_text(lua_State* L) {
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        if (auto* buf = ed->GetBuffer()) {
            lua_pushstring(L, buf->GetText().c_str());
            return 1;
        }
    }
    lua_pushstring(L, "");
    return 1;
}

/// luce.set_text(text)
static int l_set_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        if (auto* buf = ed->GetBuffer()) {
            int line_count = buf->GetLineCount();
            if (line_count > 0) {
                int last_len = static_cast<int>(buf->GetLine(line_count - 1).size());
                buf->DeleteRange(0, 0, line_count - 1, last_len);
            }
            buf->InsertText(0, 0, text);
        }
    }
    return 0;
}

/// luce.get_line_count() → int
static int l_get_line_count(lua_State* L) {
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        if (auto* buf = ed->GetBuffer()) {
            lua_pushinteger(L, buf->GetLineCount());
            return 1;
        }
    }
    lua_pushinteger(L, 0);
    return 1;
}

/// luce.get_line(n) → string (0-indexed)
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

/// luce.set_line(n, text) (0-indexed)
static int l_set_line(lua_State* L) {
    int line = static_cast<int>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        if (auto* buf = ed->GetBuffer()) {
            if (line >= 0 && line < buf->GetLineCount()) {
                int len = static_cast<int>(buf->GetLine(line).size());
                buf->DeleteRange(line, 0, line, len);
                buf->InsertText(line, 0, text);
            }
        }
    }
    return 0;
}

/// luce.replace_range(start_line, start_col, end_line, end_col, text) (0-indexed)
static int l_replace_range(lua_State* L) {
    int l1 = static_cast<int>(luaL_checkinteger(L, 1));
    int c1 = static_cast<int>(luaL_checkinteger(L, 2));
    int l2 = static_cast<int>(luaL_checkinteger(L, 3));
    int c2 = static_cast<int>(luaL_checkinteger(L, 4));
    const char* text = luaL_checkstring(L, 5);

    App* app = GetApp(L);
    if (auto* ed = GetActiveEditor(app)) {
        if (auto* buf = ed->GetBuffer()) {
            buf->DeleteRange(l1, c1, l2, c2);
            buf->InsertText(l1, c1, text);
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
        ed->EnsureCursorVisible();
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

/// luce.open_file(path)
static int l_open_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    App* app = GetApp(L);
    if (app) {
        app->GetTabBar().OpenFile(path, &app->GetThemeManager().Active());
        app->SaveSession();
    }
    return 0;
}

/// luce.save_file()
static int l_save_file(lua_State* L) {
    App* app = GetApp(L);
    if (app) {
        app->GetTabBar().SaveActive();
        app->SaveSession();
    }
    return 0;
}

/// luce.close_tab()
static int l_close_tab(lua_State* L) {
    App* app = GetApp(L);
    if (app) {
        app->GetTabBar().CloseTab(app->GetTabBar().ActiveIndex());
        app->SaveSession();
    }
    return 0;
}

/// luce.get_workspace_path() → string
static int l_get_workspace_path(lua_State* L) {
    App* app = GetApp(L);
    if (app) {
        lua_pushstring(L, app->GetFileExplorer().Root().c_str());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

/// luce.execute_command(cmd) → { exit_code = int, output = string }
static int l_execute_command(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    auto res = platform::RunCommand(cmd);
    lua_newtable(L);
    lua_pushinteger(L, res.exit_code);
    lua_setfield(L, -2, "exit_code");
    lua_pushstring(L, res.output.c_str());
    lua_setfield(L, -2, "output");
    return 1;
}

/// luce.add_diagnostic({ file = "...", line = 1, column = 1, message = "...", severity = "error"|"warning"|"info" })
static int l_add_diagnostic(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    App* app = GetApp(L);

    std::string file_path;
    lua_getfield(L, 1, "file");
    if (lua_isstring(L, -1)) {
        file_path = lua_tostring(L, -1);
    } else if (app && app->GetTabBar().ActiveTab()) {
        file_path = app->GetTabBar().ActiveTab()->filepath;
    }
    lua_pop(L, 1);

    int line = 1;
    lua_getfield(L, 1, "line");
    if (lua_isinteger(L, -1)) line = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    int col = 1;
    lua_getfield(L, 1, "column");
    if (lua_isinteger(L, -1)) col = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    std::string message = "Diagnostic from plugin";
    lua_getfield(L, 1, "message");
    if (lua_isstring(L, -1)) message = lua_tostring(L, -1);
    lua_pop(L, 1);

    DiagnosticSeverity sev = DiagnosticSeverity::Error;
    lua_getfield(L, 1, "severity");
    if (lua_isstring(L, -1)) {
        std::string s = lua_tostring(L, -1);
        if (s == "warning" || s == "warn") sev = DiagnosticSeverity::Warning;
        else if (s == "info") sev = DiagnosticSeverity::Info;
    }
    lua_pop(L, 1);

    Diagnostic diag;
    diag.file_path = file_path;
    diag.line = line;
    diag.column = col;
    diag.message = message;
    diag.severity = sev;
    diag.origin_file = file_path;

    DiagnosticManager::Instance().AddDiagnostic(diag);
    return 0;
}

/// luce.clear_diagnostics([file])
static int l_clear_diagnostics(lua_State* L) {
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        std::string file = lua_tostring(L, 1);
        DiagnosticManager::Instance().ClearDiagnosticsForFile(file);
    } else {
        DiagnosticManager::Instance().ClearDiagnostics();
    }
    return 0;
}

/// luce.register_completion_provider(extension, callback_fn)
static int l_register_completion_provider(lua_State* L) {
    const char* ext = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_getfield(L, LUA_REGISTRYINDEX, "_luce_comp_providers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "_luce_comp_providers");
    }

    lua_pushvalue(L, 2);
    lua_setfield(L, -2, ext);
    lua_pop(L, 1);
    return 0;
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
    lua_pushlightuserdata(L_, static_cast<void*>(app));
    lua_setfield(L_, LUA_REGISTRYINDEX, "_luce_app");

    static const luaL_Reg luce_funcs[] = {
        { "register_command",            l_register_command            },
        { "on",                          l_on                          },
        { "insert_text",                 l_insert_text                 },
        { "get_text",                    l_get_text                    },
        { "set_text",                    l_set_text                    },
        { "get_line_count",              l_get_line_count              },
        { "get_line",                    l_get_line                    },
        { "set_line",                    l_set_line                    },
        { "replace_range",               l_replace_range               },
        { "delete_selection",            l_delete_selection            },
        { "get_selection",               l_get_selection               },
        { "get_cursor_line",             l_get_cursor_line             },
        { "get_cursor_column",           l_get_cursor_col              },
        { "set_cursor",                  l_set_cursor                  },
        { "get_file_path",               l_get_file_path               },
        { "get_file_name",               l_get_file_name               },
        { "get_file_extension",          l_get_file_extension          },
        { "open_file",                   l_open_file                   },
        { "save_file",                   l_save_file                   },
        { "close_tab",                   l_close_tab                   },
        { "get_workspace_path",          l_get_workspace_path          },
        { "execute_command",             l_execute_command             },
        { "add_diagnostic",              l_add_diagnostic              },
        { "clear_diagnostics",           l_clear_diagnostics           },
        { "register_completion_provider",l_register_completion_provider},
        { "set_status",                  l_set_status                  },
        { "show_notification",           l_show_notification           },
        { "show_error",                  l_show_error                  },
        { "show_warning",                l_show_warning                },
        { "show_info",                   l_show_info                   },
        { "log",                         l_log                         },
        { "warn",                        l_warn                        },
        { nullptr,                       nullptr                       }
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
    if (!L_ || !enabled_ || !has_tick_) return;
    lua_getglobal(L_, "on_tick");
    if (lua_isfunction(L_, -1)) {
        lua_pushnumber(L_, static_cast<lua_Number>(delta_time));
        if (lua_pcall(L_, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L_, -1);
            std::cerr << "[Lua Plugin Tick Error] " << (err ? err : "?") << "\n";
            lua_pop(L_, 1);
            has_tick_ = false;
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

void LuaPlugin::DispatchEvent(const std::string& event_name) {
    if (!L_ || !enabled_) return;
    lua_getfield(L_, LUA_REGISTRYINDEX, "_luce_events");
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return;
    }
    lua_getfield(L_, -1, event_name.c_str());
    if (lua_istable(L_, -1)) {
        int count = static_cast<int>(lua_rawlen(L_, -1));
        for (int i = 1; i <= count; ++i) {
            lua_rawgeti(L_, -1, i);
            if (lua_isfunction(L_, -1)) {
                if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L_, -1);
                    std::cerr << "[Lua Plugin Event Error (" << event_name << ")] " << (err ? err : "?") << "\n";
                    lua_pop(L_, 1);
                }
            } else {
                lua_pop(L_, 1);
            }
        }
    }
    lua_pop(L_, 2);
}

void LuaPlugin::DispatchEvent(const std::string& event_name, const std::string& str_arg) {
    if (!L_ || !enabled_) return;
    lua_getfield(L_, LUA_REGISTRYINDEX, "_luce_events");
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return;
    }
    lua_getfield(L_, -1, event_name.c_str());
    if (lua_istable(L_, -1)) {
        int count = static_cast<int>(lua_rawlen(L_, -1));
        for (int i = 1; i <= count; ++i) {
            lua_rawgeti(L_, -1, i);
            if (lua_isfunction(L_, -1)) {
                lua_pushstring(L_, str_arg.c_str());
                if (lua_pcall(L_, 1, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L_, -1);
                    std::cerr << "[Lua Plugin Event Error (" << event_name << ")] " << (err ? err : "?") << "\n";
                    lua_pop(L_, 1);
                }
            } else {
                lua_pop(L_, 1);
            }
        }
    }
    lua_pop(L_, 2);
}

void LuaPlugin::DispatchEvent(const std::string& event_name, int int_arg1, int int_arg2) {
    if (!L_ || !enabled_) return;
    lua_getfield(L_, LUA_REGISTRYINDEX, "_luce_events");
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return;
    }
    lua_getfield(L_, -1, event_name.c_str());
    if (lua_istable(L_, -1)) {
        int count = static_cast<int>(lua_rawlen(L_, -1));
        for (int i = 1; i <= count; ++i) {
            lua_rawgeti(L_, -1, i);
            if (lua_isfunction(L_, -1)) {
                lua_pushinteger(L_, int_arg1);
                lua_pushinteger(L_, int_arg2);
                if (lua_pcall(L_, 2, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L_, -1);
                    std::cerr << "[Lua Plugin Event Error (" << event_name << ")] " << (err ? err : "?") << "\n";
                    lua_pop(L_, 1);
                }
            } else {
                lua_pop(L_, 1);
            }
        }
    }
    lua_pop(L_, 2);
}

std::vector<std::string> LuaPlugin::GetCompletions(const std::string& ext, const std::string& prefix, int line, int col) {
    std::vector<std::string> results;
    if (!L_ || !enabled_) return results;

    lua_getfield(L_, LUA_REGISTRYINDEX, "_luce_comp_providers");
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return results;
    }

    // Check provider for specific extension or wildcard "*"
    lua_getfield(L_, -1, ext.c_str());
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        lua_getfield(L_, -1, "*");
    }

    if (lua_isfunction(L_, -1)) {
        lua_pushstring(L_, prefix.c_str());
        lua_pushinteger(L_, line);
        lua_pushinteger(L_, col);
        if (lua_pcall(L_, 3, 1, 0) == LUA_OK) {
            if (lua_istable(L_, -1)) {
                int len = static_cast<int>(lua_rawlen(L_, -1));
                for (int i = 1; i <= len; ++i) {
                    lua_rawgeti(L_, -1, i);
                    if (lua_isstring(L_, -1)) {
                        results.push_back(lua_tostring(L_, -1));
                    }
                    lua_pop(L_, 1);
                }
            }
            lua_pop(L_, 1);
        } else {
            const char* err = lua_tostring(L_, -1);
            std::cerr << "[Lua Completion Error] " << (err ? err : "?") << "\n";
            lua_pop(L_, 1);
        }
    } else {
        lua_pop(L_, 1);
    }

    lua_pop(L_, 1);
    return results;
}

}  // namespace luce
