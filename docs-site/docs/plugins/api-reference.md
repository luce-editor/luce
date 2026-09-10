---
id: api-reference
title: Lua Plugin API Reference
sidebar_label: API Reference
slug: /plugins/api-reference
---

# Lua Plugin API Reference

Luce features a rich, zero-compilation **Lua 5.4 Plugin Architecture (API 2.0)**. Plug-ins can react to editor lifecycle events, modify buffers, register diagnostics/linters, provide autocomplete suggestions, and interact with the host operating system.

All functions are exposed under the global `luce` table.

---

## Plugin Structure & Packages

Luce supports two plugin distribution formats in the `plugins/` directory:
1. **Single-file script**: `plugins/my_plugin.lua`
2. **Directory package**: `plugins/my_package/init.lua` (with optional `manifest.json` or sub-modules)

Plugins can be dynamically reloaded or toggled enabled/disabled at runtime from the **Plugins** sidebar tab.

---

## Event Hooks (`luce.on`)

Register callback functions for editor lifecycle events:

```lua
luce.on(event_name, callback)
```

| Event Name | Callback Arguments | Description |
|---|---|---|
| `"file_opened"` | `(filepath)` | Fired immediately after an existing file is loaded into a tab. |
| `"before_save"` | `(filepath)` | Fired right before buffer is written to disk. Ideal for formatters or headers. |
| `"after_save"` | `(filepath)` | Fired after successful save. Ideal for background linters or build tools. |
| `"cursor_moved"`| `(line, column)` | Fired when primary caret moves (0-indexed). |
| `"text_changed"`| `(start_line, count)` | Fired when text content is modified in the buffer. |

### Example: Format On Save
```lua
luce.on("before_save", function(path)
    -- Remove trailing whitespace across document
    local text = luce.get_text()
    local cleaned = text:gsub("[ \t]+\n", "\n")
    if cleaned ~= text then
        luce.set_text(cleaned)
    end
end)
```

---

## Document & Buffer Editing

### `luce.get_text() → string`
Returns the entire text of the active editor buffer.

### `luce.set_text(text)`
Replaces the entire document buffer (with undo/redo preservation).

### `luce.get_line_count() → number`
Returns the total number of lines in the active document.

### `luce.get_line(n) → string`
Returns the text of line `n` (0-indexed).

### `luce.set_line(n, text)`
Sets the text of line `n` (0-indexed).

### `luce.replace_range(start_line, start_col, end_line, end_col, text)`
Replaces an exact coordinate range with new text.

### `luce.insert_text(text)`
Inserts text at the current cursor position.

### `luce.delete_selection()`
Deletes selected text.

### `luce.get_selection() → string`
Returns the currently selected text.

---

## Workspace & File Controls

### `luce.open_file(path)`
Opens the specified absolute file path in a new editor tab (or switches to it if already open).

### `luce.save_file()`
Saves the currently active document to disk.

### `luce.close_tab()`
Closes the currently active editor tab.

### `luce.get_file_path() → string`
Returns the absolute path of the open file (e.g. `"C:/Projects/app/main.cpp"`).

### `luce.get_file_name() → string`
Returns the filename with extension (e.g. `"main.cpp"`).

### `luce.get_file_extension() → string`
Returns the file extension with leading dot (e.g. `".cpp"`).

### `luce.get_workspace_path() → string`
Returns the root directory path of the currently open project.

### `luce.execute_command(command) → string`
Runs an external shell command synchronously in the workspace directory and returns its output:
```lua
local formatted = luce.execute_command("clang-format src/main.cpp")
```

---

## Commands & Shortcuts

### `luce.register_command(id, display_name, [shortcut], fn)`
Registers a command visible in the Command Palette (`Ctrl+Shift+P`).

| Parameter | Type | Description |
|---|---|---|
| `id` | `string` | Unique command identifier (e.g. `"my_plugin.run"`) |
| `display_name` | `string` | Human-readable title shown in Command Palette |
| `shortcut` | `string` *(optional)* | Shortcut hint (e.g. `"Ctrl+Alt+F"`) |
| `fn` | `function` | Handler invoked upon command execution |

```lua
luce.register_command("tools.format", "Format Document", "Ctrl+Alt+F", function()
    luce.show_notification("Formatting document...", "info")
end)
```

---

## Custom Diagnostics API (Linters)

Plugins can submit error squiggles and diagnostics directly into Luce's Problems dock panel:

### `luce.add_diagnostic(diag_table)`
```lua
luce.add_diagnostic({
    file     = "C:/Project/src/main.cpp",
    line     = 42,            -- 1-indexed
    column   = 5,             -- 1-indexed
    message  = "Unused variable 'x'",
    severity = "warning"      -- "error" | "warning" | "info"
})
```

### `luce.clear_diagnostics([file_path])`
Clears diagnostics for a specific file (or all files if no parameter is provided).

---

## Autocomplete Providers

Register language-specific completion suggestion handlers:

```lua
luce.register_completion_provider(".cpp", function(prefix, line, col)
    if prefix:sub(1, 1) == "s" then
        return { "std::string", "std::vector", "size_t", "static_cast<>()" }
    end
    return {}
end)
```

---

## Toast Notifications & Logging

- `luce.show_notification(message, [level], [duration])`: Displays floating toast notification (`"info"`, `"warn"`, `"error"`, `"success"`).
- `luce.show_info(msg)` / `luce.show_warning(msg)` / `luce.show_error(msg)`
- `luce.set_status(msg)`: Sets status bar text.
- `luce.log(msg)` / `luce.warn(msg)`: Prints formatted diagnostic logs.

---

## Lifecycle Callbacks

- `function on_tick(dt)`: Called every frame (`dt` is delta time in seconds).
- `function on_shutdown()`: Called when the editor exits or when plugin is reloaded.
