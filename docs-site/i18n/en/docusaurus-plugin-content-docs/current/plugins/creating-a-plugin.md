---
id: creating-a-plugin
title: Creating a Plugin
sidebar_label: Plugin Tutorial
slug: /plugins/creating-a-plugin
---

# Creating a Lua Plugin for Luce

Luce supports **Lua 5.4** scripts as plugins — no compilation, no CMake, no setup required.
Just write a `.lua` file and drop it into the `plugins/` folder next to `luce.exe`.

---

## Minimal Plugin

```lua
-- plugins/hello.lua

-- Plugin metadata (optional but recommended)
luce.plugin = {
    name        = "Hello Plugin",
    version     = "1.0.0",
    author      = "Your Name",
    description = "Shows a greeting in the status bar.",
}

-- Register a command visible in the Command Palette (Ctrl+Shift+P)
luce.register_command("hello", "Hello: Greet the World", function()
    luce.set_status("Hello, World!")
    luce.log("Hello plugin executed!")
end)
```

---

## Plugin Lifecycle

| Stage               | When it runs                          |
|---------------------|---------------------------------------|
| File executed       | On load (editor startup)              |
| `on_tick(dt)`       | Every frame (optional)                |
| `on_shutdown()`     | When the editor closes (optional)     |

```lua
-- Optional lifecycle callbacks

function on_tick(dt)
    -- dt = time since last frame in seconds
end

function on_shutdown()
    luce.log("My plugin is shutting down.")
end
```

---

## Full API — the `luce` table

### Commands

```lua
luce.register_command(id, display_name, function() ... end)
```
Registers a command in the Command Palette (`Ctrl+Shift+P`).

---

### Text Editing

```lua
luce.insert_text(text)       -- insert text at cursor position
luce.delete_selection()      -- delete selected text
luce.get_selection()         -- returns selected text (string)
luce.get_line(n)             -- returns text of line n (0-indexed)
```

### Cursor & File

```lua
luce.get_cursor_line()       -- current line number (0-indexed)
luce.get_cursor_column()     -- current column number (0-indexed)
luce.set_cursor(line, col)   -- move cursor to position
luce.get_file_path()         -- path to the currently open file
luce.get_file_name()         -- returns file name with extension (e.g. main.cpp)
luce.get_file_extension()    -- returns file extension (e.g. .cpp, .html)
```

### Status & Logging

```lua
luce.set_status(text)        -- show text in the status bar
luce.log(text)               -- log info to console [Lua Plugin INFO]
luce.warn(text)              -- log warning to console [Lua Plugin WARN]
```

---

## Example: Convert Selection to UPPERCASE

```lua
-- plugins/uppercase.lua
luce.plugin = {
    name        = "UPPERCASE Converter",
    version     = "1.0.0",
    author      = "Luce Team",
    description = "Converts selected text to UPPERCASE.",
}

luce.register_command("uppercase", "UPPERCASE: Convert Selection", function()
    local text = luce.get_selection()
    if text == "" then
        luce.set_status("No text selected!")
        return
    end
    luce.delete_selection()
    luce.insert_text(text:upper())
end)
```

---

## How to Install a Plugin

1. Copy the `.lua` file into the `plugins/` folder next to `luce.exe`.
2. Launch (or restart) the editor.
3. Open `Ctrl+Shift+P` and type the command name registered in the plugin.

> **Tip:** Click the **"Open Plugins Folder"** button in the Plugins tab of the sidebar to open the folder directly in Windows Explorer.
