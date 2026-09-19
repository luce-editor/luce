---
id: overview
title: Interface & Layout
sidebar_label: Overview
slug: /interface/overview
---

# Interface & Layout Management

Luce's interface is built on **Dear ImGui (docking branch)** combined with the minimalist aesthetic of modern code editors.

---

## Workspace Areas

1. **Menu Bar**: Global actions for files, editing, view toggles, themes, and developer tools.
2. **Sidebar**: Features a horizontal Activity Bar switching between the File Explorer tree, Source Control (Git), and loaded Lua Plugins.
3. **Tab Bar & Editor**: Multi-file editing tabs, Split View side-by-side editing (`Ctrl+\`), real-time Markdown preview (`Ctrl+Shift+M`), minimap overview (`Ctrl+M`), Git Gutter diff markers, and built-in image viewer.
4. **Command Palette & Search**: Fast keyboard-driven command runner with `>` prompt (`Ctrl+Shift+P`), Quick Open (`Ctrl+P`), and Project-Wide Search (`Ctrl+Shift+F`).
5. **Bottom Dock Panel**:
   - **Problems**: Central diagnostic table displaying workspace errors and warnings with copy actions.
   - **Output**: Dedicated read-only output logs from tools and background jobs.
   - **Terminal**: Embedded multi-tab terminal with VT100 emulation and PowerShell/Bash integration.
6. **Status Bar**: Live diagnostics including dynamic theme-styled Git branch status, language mode, cursor position (Ln, Col), file encoding, indentation style, and application version.
7. **Preferences & Settings**: Independent native OS settings window (`Ctrl+,`) inspired by Zed with animated toggle switches, theme gallery, and direct JSON editing (`Ctrl+Shift+,`) with real-time hot-reloading.

---

## Split View (Side-by-Side Editing)

Pressing `Ctrl+\` or selecting **View: Toggle Split Editor** divides the editor area into two independent, synchronized panes:
- Edit different files simultaneously (e.g. `main.cpp` on the left and `test.hpp` on the right).
- View code side-by-side with live Markdown previews or documentation.
- Independent scrolling, selection, cursor blinking, and language autocompletion in each pane.
- Rapid file selection via the top pane dropdown or Explorer double-click.

---

## Drag & Drop System

Luce features a comprehensive, fluid mouse drag-and-drop system:
- **Tab Reordering**: Grab any tab header in the tab bar and drag it horizontally to rearrange tabs. Drop onto the `+` button to append.
- **Drag Tab to Split**: Drag a tab from the tab bar toward the right side (35% drop zone) of the editor canvas to automatically split the view into two panes with that file on the right.
- **Pane-to-Pane Routing**: When Split View is active, drag tabs or files directly into either the Left Pane or Right Pane to view them in that pane.
- **File Explorer Drag & Drop**: Drag files from the workspace tree into the tab bar to open at a specific position, into the editor to open/split, or into the terminal panel to paste their path.
- **Visual Overlays**: Modern translucent blue drop targets with accent borders and badges (`Split Right`, `Left Pane`, `Right Pane`) clearly indicate drop destinations.

---

## Docking & Session Persistence

- **Panel Docking**: Window arrangements and docking states are preserved in `imgui.ini`.
- **Workspace Sessions**: The open folder, active tabs, and interface scaling factor are automatically saved to `session.json` and restored seamlessly on startup.
