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
2. **Sidebar**: Features a horizontal Activity Bar switching between the File Explorer tree and loaded Lua Plugins.
3. **Tab Bar & Editor**: Multi-file editing tabs, horizontal/vertical splits, real-time Markdown preview (`Ctrl+Shift+M`), and built-in image viewer.
4. **Bottom Dock Panel**:
   - **Problems**: Central diagnostic table displaying workspace errors and warnings. Double-clicking any problem instantly opens the source file and navigates directly to the specified line.
   - **Output**: Dedicated read-only output logs from tools and background jobs.
   - **Terminal**: Embedded multi-tab terminal with VT100 emulation and PowerShell/Bash integration.
5. **Status Bar**: Live diagnostics including language mode, cursor position (Ln, Col), file encoding, indentation style, and application version.

---

## Docking & Session Persistence

- **Panel Docking**: Window arrangements and docking states are preserved in `imgui.ini`.
- **Workspace Sessions**: The open folder, active tabs, and interface scaling factor are automatically saved to `session.json` and restored seamlessly on startup.
