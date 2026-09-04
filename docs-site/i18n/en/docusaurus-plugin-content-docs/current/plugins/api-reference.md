---
id: api-reference
title: Lua Plugin API Reference
sidebar_label: API Reference
slug: /plugins/api-reference
---

# Lua Plugin API Reference

All functions available to Lua scripts are exposed in the global `luce` table.

---

## Plugin Metadata

An optional table defining metadata shown in the Plugins panel:

```lua
luce.plugin = {
    name        = "My Plugin",       -- shown in the Plugins panel
    version     = "1.0.0",           -- version string
    author      = "Your Name",       -- author
    description = "What this plugin does.",
}
```

---

## Commands

### `luce.register_command(id, display_name, fn)`

Registers a command visible in the Command Palette (`Ctrl+Shift+P`).

| Parameter      | Type       | Description                                       |
|----------------|------------|---------------------------------------------------|
| `id`           | `string`   | Unique identifier (e.g. `"my_cmd"`)               |
| `display_name` | `string`   | Name shown in the palette (e.g. `"My: Action"`)   |
| `fn`           | `function` | Function called when the command is executed      |

```lua
luce.register_command("say_hi", "My Plugin: Say Hello", function()
    luce.insert_text("Hello!")
end)
```

---

## Text Editing

### `luce.insert_text(text)`
Inserts the given string at the current cursor position.

### `luce.delete_selection()`
Deletes the currently selected text.

### `luce.get_selection() → string`
Returns the currently selected text. Returns `""` if nothing is selected.

### `luce.get_line(n) → string`
Returns the text of line number `n` (0-indexed).

---

## Cursor

### `luce.get_cursor_line() → number`
Returns the current cursor line number (0-indexed).

### `luce.get_cursor_column() → number`
Returns the current cursor column number (0-indexed).

### `luce.set_cursor(line, col)`
Moves the cursor to the given position.

---

## File

### `luce.get_file_path() → string`
Returns the full path of the currently open file (e.g. `"C:/Projects/app/src/main.cpp"`). Returns `""` if no file is open.

### `luce.get_file_name() → string`
Returns the file name with extension of the currently open file (e.g. `"main.cpp"` or `"Untitled-1"`). Returns `""` if no editor tab is active.

### `luce.get_file_extension() → string`
Returns the file extension with leading dot of the currently open file (e.g. `".cpp"` or `".lua"`). Returns `""` if the file has no extension or is untitled.

---

## Status & Toast Notifications

### `luce.set_status(text)`
Displays a floating Toast notification in the bottom-left corner of the editor (VS Code style) and prints to the console.

### `luce.show_notification(message, [level], [duration])`
Displays a bottom-left floating Toast notification with auto-fadeout.
- `message` (`string`): the notification text.
- `level` (`string`, optional): `"info"`, `"warn"`, `"error"`, or `"success"` (default: `"info"`).
- `duration` (`number`, optional): visibility duration in seconds (default: `4.0s`).

### `luce.show_error(text)`
Displays a red error Toast notification.

### `luce.show_warning(text)`
Displays a yellow warning Toast notification.

### `luce.show_info(text)`
Displays a blue info Toast notification.

### `luce.log(text)`
Prints an info message to the console (`[Lua Plugin INFO] ...`).

### `luce.warn(text)`
Prints a warning to the console (`[Lua Plugin WARN] ...`).

---

## Lifecycle Callbacks

Optional global functions that Luce will call automatically:

### `function on_tick(dt)`
Called every frame. `dt` is the time since the last frame in seconds.

```lua
function on_tick(dt)
    -- avoid heavy work here — this is a hot path!
end
```

### `function on_shutdown()`
Called when the editor closes (or when the plugin is manually unloaded).

```lua
function on_shutdown()
    luce.log("Plugin shutting down.")
end
```
