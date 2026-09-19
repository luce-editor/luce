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

On every startup (or whenever the `↻` refresh button in the sidebar is clicked), `PluginManager` scans the `plugins/` directory next to `luce.exe`. 

Luce supports two plugin formats:
1. **Directory packages (recommended)**: A directory containing `init.lua`, an optional `README.md` documentation file, `manifest.json`, plus custom icons or sub-modules.
2. **Single-file scripts**: Standalone `.lua` files.

```
build/Release/
├── luce.exe
└── plugins/
    ├── cpp_snippets/           ← directory package
    │   ├── init.lua            ← primary entry point
    │   └── README.md           ← extension documentation
    ├── json_intellisense/      ← directory package
    │   ├── init.lua
    │   └── README.md
    └── uppercase.lua           ← single-file script
```

Each plugin is automatically loaded into its own isolated and safe Lua environment.

---

## Sidebar Plugins View

Plugin management is handled in the dedicated Plugins panel located in the Sidebar:

- **Activity Bar Icon**: The third icon in the horizontal activity bar (next to Explorer and Git) toggles the Plugins panel.
- **Search Bar (`Search plugins...`)**: Real-time filtering by plugin name, description, or author.
- **Live Hot-Reloading (`↻` / `Reload Plugins`)**: The header reload button immediately re-scans the `plugins/` directory and hot-reloads all scripts in memory without needing to restart the editor.
- **Plugin List Item Features**:
  - Plugin icon (custom or default SVG plugin icon).
  - Name, version, author, and concise description.
  - **Enable/Disable Checkbox**: Quickly activate or deactivate a plugin without deleting files.
  - **Uninstall Button (Trash Can)**: Safely uninstall and delete the plugin directory with a confirmation dialog.
  - **Item Click**: Opens the full Extension Details page in a new editor tab.

---

## Extension Details Tab View

Clicking any plugin in the list opens a full details tab styled like the VS Code Extension Marketplace:

- **Clean Native Tab**: The tab header features the plugin's SVG icon and title `Extension: <Name>`.
- **Hero Section**:
  - High-resolution extension icon, title, author, version, and type badge (e.g. `Lua Plugin`).
  - **Uninstall** button: Delete plugin from disk.
  - **Open Folder** button: Open the plugin directory in Windows Explorer.
  - **Edit Script** button: Open `init.lua` in the code editor for immediate inspection or modification.
- **DETAILS Tab**: Built-in Markdown renderer parsing the extension's `README.md` in real-time with headers, lists, and code blocks.
- **MORE INFO Side Panel**: Quick summary of identifier, version, author, file path, and documentation source.

---

## Environment Isolation

Each plugin gets its own **`lua_State`** (isolated VM). This means:

- A crash or error in one script does not affect others.
- Plugins cannot see each other's global variables.
- Each plugin has its own copy of the Lua standard library (`os`, `string`, `math`, `table`, etc.).

---

## API Registration

After loading a file, Luce automatically provides a global `luce` table containing the full editor API.
Scripts can call `luce.register_command(...)`, `luce.register_completion(...)`, or `luce.on(...)` — no imports needed.

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
      CommandPalette / TabBar / Editor / ...
```

---

## Plugin Lifecycle

| Stage        | What happens                                                        |
|--------------|---------------------------------------------------------------------|
| **Init**     | `luaL_dofile()` — the entry point (`init.lua` or `.lua`) is executed |
| **Register** | Script calls `luce.register_command(...)`, registers hooks or completions |
| **Tick**     | Luce calls global `on_tick(dt)` every frame (if defined)           |
| **Shutdown** | Luce calls `on_shutdown()` on exit, then closes the `lua_State`    |

---

## Installing a New Plugin

1. Create a folder `plugins/<my_plugin>/` with `init.lua` (and optional `README.md`), or a single file `plugins/<my_plugin>.lua`.
2. Click the **`↻`** button in the Plugins sidebar panel (or run the reload command via Command Palette).
3. The plugin immediately appears in the list and runs without restarting Luce!

> Click **"Open Plugins Folder"** in the Plugins tab of the sidebar to quickly open the folder in Windows Explorer.
