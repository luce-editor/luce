---
id: creating-a-plugin
title: Creating Custom Plugins (Lua)
sidebar_label: Plugin Tutorial
slug: /plugins/creating-a-plugin
---

# Creating Custom Plugins (Lua)

Luce allows extending the editor using the simple and lightweight [Lua 5.4](https://www.lua.org/) scripting language — no compilation, no CMake, and no external dependencies required.

Plugin `.lua` files reside in the `plugins/` directory next to the `luce.exe` executable.

---

## Practical Plugin Example (`plugins/skeletonCpp.lua`)

The script below registers two commands in the Command Palette: inserting a basic C++ `main()` function skeleton and dynamically generating a class header based on the active file name. It also demonstrates text buffer manipulation and toast notifications.

```lua
-- ============================================================================
--  skeletonCpp.lua -> insert basic C++ skeleton at cursor
--
--  Install: drop this file into plugins/ folder next to luce.exe
--  Use: Ctrl+Shift+P -> Skeleton: Insert Basic Skeleton
-- ============================================================================

-- 1. Plugin metadata (displayed in the Plugins panel)
luce.plugin = {
    name = "Skeleton",
    version = "1.0.0",
    author = "Luce Team",
    description = "Inserts basic C++ skeleton and class header"
}

-- 2. Function inserting a standard main() function
local function insert_skel()
    local skelMesh = [[
#include <iostream>

int main() {
    
    return 0;
}
]]
    luce.insert_text(skelMesh)
    local file_name = luce.get_file_name()
    luce.show_notification(string.format("Skeleton inserted for %s", file_name), "success", 5)
end

-- 3. Function generating a class header matched to the file name
local function insert_header_skel()
    local file_name = luce.get_file_name()
    local pos = string.find(file_name, "%.")
    local class_name = pos and string.sub(file_name, 1, pos - 1) or file_name
    local skelMesh = string.format([[
#pragma once

class %s {
public:
    %s();
    ~%s();
private:

};
]], class_name, class_name, class_name)

    luce.insert_text(skelMesh)
    luce.show_notification(string.format("Header inserted for %s", file_name), "success", 5)
end

-- 4. Register commands in the Command Palette (Ctrl+Shift+P)
luce.register_command("insert_skeleton", "Skeleton: Insert Basic Skeleton", insert_skel)
luce.register_command("insert_header_skel", "Skeleton: Insert Header Skeleton", insert_header_skel)
```

---

## Plugin Lifecycle

Each Lua script is loaded into an isolated `lua_State` VM. In addition to the code evaluated immediately on startup, a plugin can optionally define lifecycle callbacks:

| Callback           | When it runs                   | Purpose |
|--------------------|--------------------------------|---------|
| *Top-level code*   | When the plugin is loaded      | Command registration and initial setup |
| `on_tick(dt)`      | Every render frame             | Periodic status checks, timers (`dt` = seconds) |
| `on_shutdown()`    | Right before editor closes     | Resource cleanup or persisting state |

```lua
function on_tick(dt)
    -- Called every frame (e.g. for background timers)
end

function on_shutdown()
    luce.log("Plugin is cleaning up before Luce exits.")
end
```

---

## Handy API Cheat Sheet

Core functions provided by the global `luce` table:

### Registering Commands
```lua
luce.register_command(id, display_name, function() ... end)
```

### Text Editing
```lua
luce.insert_text(text)       -- Insert text at cursor position
luce.delete_selection()      -- Delete current text selection
luce.get_selection()         -- Returns selected text (string)
luce.get_line(n)             -- Returns line n text (0-indexed)
```

### Cursor & File
```lua
luce.get_cursor_line()       -- Current line number (0-indexed)
luce.get_cursor_column()     -- Current column number (0-indexed)
luce.set_cursor(line, col)   -- Move cursor to line and column
luce.get_file_path()         -- Full path to the active file
luce.get_file_name()         -- File name with extension (e.g. main.cpp)
luce.get_file_extension()    -- File extension without dot (e.g. cpp)
```

### Notifications, Status & Logging
```lua
luce.show_notification(msg, type, duration) -- Toast ("info", "success", "warning", "error")
luce.set_status(text)                       -- Display text in the status bar
luce.log(text)                              -- Log to terminal [Lua Plugin INFO]
luce.warn(text)                             -- Warning to terminal [Lua Plugin WARN]
```

> For full parameter signatures and type documentation, see **[Lua API Reference](./api-reference)**.

---

## How to Install a Plugin in Luce

To install and use a new plugin without restarting the editor:

1. Open Luce and switch to the **Plugins** tab (puzzle icon in the Activity Bar).
2. Click the **Open Plugins Folder** button at the bottom of the panel to open `plugins/` in your system file manager.
3. Copy or save your `.lua` file into this folder.
4. Click the **Reload Plugins** button in the editor.
5. Your plugin will immediately appear in the list, and its commands will be ready in the **Command Palette** (`Ctrl+Shift+P`).
