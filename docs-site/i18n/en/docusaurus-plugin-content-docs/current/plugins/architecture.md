---
id: architecture
title: Plugin System Architecture
sidebar_label: Architecture
slug: /plugins/architecture
---

# Plugin System Architecture

Luce uses **Lua 5.4** as the scripting engine for plugins. Every plugin is a plain `.lua` file — no compilation, no CMake, no configuration.

---

## How Plugin Loading Works

On every startup, `PluginManager` scans the `plugins/` directory next to `luce.exe`. Each `.lua` file found is loaded into its own isolated Lua environment.

```
build/Release/
├── luce.exe
└── plugins/
    ├── uppercase.lua     ← automatically loaded
    ├── timestamp.lua     ← automatically loaded
    └── my_plugin.lua     ← automatically loaded
```

---

## Environment Isolation

Each plugin gets its own **`lua_State`** (isolated VM). This means:

- A crash or error in one script does not affect others.
- Plugins cannot see each other's global variables.
- Each plugin has its own copy of the Lua standard library (`os`, `string`, `math`, `table`, etc.).

---

## API Registration

After loading a file, Luce automatically provides a global `luce` table containing the full editor API.
Scripts can call `luce.register_command(...)`, `luce.insert_text(...)`, etc. — no imports needed.

```
lua_State (plugin A)          lua_State (plugin B)
┌──────────────────────┐      ┌──────────────────────┐
│  luce.* API (C++)    │      │  luce.* API (C++)     │
│  on_tick / on_shutdown│      │  on_tick / on_shutdown│
│  globals of plugin A │      │  globals of plugin B  │
└──────────────────────┘      └──────────────────────┘
         │                              │
         └──────────┬───────────────────┘
                    │
             PluginManager (C++)
                    │
            CommandPalette / TabBar / ...
```

---

## Plugin Lifecycle

| Stage        | What happens                                                        |
|--------------|---------------------------------------------------------------------|
| **Init**     | `luaL_dofile()` — the `.lua` file is executed top-to-bottom        |
| **Register** | Script calls `luce.register_command(...)` during execution          |
| **Tick**     | Luce calls global `on_tick(dt)` every frame (if defined)           |
| **Shutdown** | Luce calls `on_shutdown()` on exit, then closes the `lua_State`    |

---

## Installing a Plugin

1. Write a `.lua` file implementing the plugin.
2. Copy it into the `plugins/` folder next to `luce.exe`.
3. Restart Luce — the plugin is loaded automatically.

> Click **"Open Plugins Folder"** in the Plugins tab of the sidebar to quickly open the folder in Windows Explorer.
