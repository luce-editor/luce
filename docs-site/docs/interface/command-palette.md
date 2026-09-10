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
| `Ctrl+Shift+P` | **Commands** | Prompted with `>`. Search and execute any editor, theme, or Lua plugin command |
| `Ctrl+P` | **Quick Open** | Rapidly fuzzy-search and jump to any project file |
| `Ctrl+Shift+F` | **Project Search** | Find in Files across the entire project with line previews and instant jump |
| `Ctrl+G` | **Go to Line** | Jump directly to a specified line number (prefixed with `:`) |

---

## Command Prompt `>` & Dynamic Mode Switching

The input box dynamically switches modes as you type, matching modern editor workflows:
- **`>` Prefix for Commands**: Opening with `Ctrl+Shift+P` pre-populates the `>` prompt. Removing the `>` automatically switches to file search (*Quick Open*). Typing `>` at the beginning switches back to commands.
- **`:` Prefix for Line Jump**: Typing `:` switches directly into *Go to Line* mode.
- **`?` Prefix or `Ctrl+Shift+F` for Project Search**: Widescreen search across all source files in the project.

---

## Project-Wide Search (`Ctrl+Shift+F`)

Pressing `Ctrl+Shift+F` activates **Find in Files**:
- **Deep Workspace Indexing**: Scans all readable source files across the workspace while ignoring binary files, `.git`, `build`, and `node_modules`.
- **Two-Line Result Layout**: Shows relative file paths and line numbers on top, with trimmed syntax snippets underneath.
- **Instant Navigation**: Selecting a match with `Enter` or mouse click opens the target file in the editor and scrolls the cursor directly to the matched line and column.

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
