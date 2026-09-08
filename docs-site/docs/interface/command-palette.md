---
id: command-palette
title: Command Palette & Quick Open
sidebar_label: Command Palette
slug: /interface/command-palette
---

# Command Palette & Quick Open

The **Command Palette** is Luce's central navigation and action hub, providing rapid keyboard-first access to all editor commands, project files, and line jumps.

---

## Modes & Keybindings

| Shortcut | Mode | Purpose |
| :--- | :--- | :--- |
| `Ctrl+Shift+P` | **Commands** | Search and execute any editor, theme, or Lua plugin command |
| `Ctrl+P` | **Quick Open** | Rapidly fuzzy-search and jump to any project file |
| `Ctrl+G` | **Go to Line** | Jump directly to a specified line number (prefixed with `:`) |

---

## Navigation & UX Features

- **Smooth Auto-Scrolling**: Navigating with `Up` and `Down` arrow keys automatically scrolls the results list to keep the currently highlighted item centered and visible.
- **Fuzzy Search Matching**: Subsequence matching filters commands and project paths on the fly as you type (e.g. `term` matches `Toggle Terminal`).
- **Clean Aesthetic**: Integrated with the theme's background and focus styling, free of intrusive highlight borders.

---

## Registering Commands in C++

In `App::RegisterCommands()` (`src/ui/app.cpp`), any internal feature or subsystem can register a named command:

```cpp
command_palette_.RegisterCommand({
    "file.save_all",
    "File: Save All",
    "Ctrl+K S", // Optional shortcut hint
    [this]() {
        for (auto& tab : tab_bar_.GetTabs()) {
            // Save logic
        }
    }
});
```
