---
id: terminal
title: Embedded Terminal
sidebar_label: Embedded Terminal
slug: /interface/terminal
---

# Embedded Terminal

Luce features a modern, fully interactive terminal panel docked at the bottom of the workspace, powered by native pseudo-consoles (ConPTY / PTY) and `libvterm`.

---

## Architecture & Features

The terminal in Luce is a complete VT100-compliant terminal emulator:
- **Emulation Engine**: Powered by `libvterm` for accurate parsing of ANSI escape codes, TrueColor palettes, and typography styles (bold, italic, underline).
- **Default Shells**:
  - **Windows**: Modern PowerShell (`pwsh.exe`) with automated fallback to Windows `powershell.exe`.
  - **Linux / macOS**: Native shell defined by `$SHELL` (e.g. `/bin/bash` or `/bin/zsh`).
- **Working Directory Sync (CWD)**: The terminal automatically launches directly inside the directory currently open in Luce's File Explorer.

---

## Multi-Session Terminal Tabs

Luce supports multiple concurrent terminal sessions:
- **Adding Tabs**: Clicking the `+` button in the terminal tab bar instantly launches a new, isolated shell process.
- **Closing Tabs**: Each tab features a close `×` button that safely terminates the corresponding subprocess.
- **Tab Numbering**: When multiple terminals are open, tabs are automatically numbered (`1: pwsh`, `2: pwsh`, etc.) to easily keep track of parallel tasks (e.g. dev servers, builds, git).
- **Active Tab Highlighting**: The active terminal tab is clearly distinguished with high-contrast text and a distinctive 2px blue accent bar (`#007ACC`) along its top edge, while inactive tabs remain muted.

---

## Color Styling & Readability

- **Command Input**: Typed commands and shell aliases render in crisp, clean white for maximum readability.
- **Inline Predictions (PSReadLine)**: Shell suggestions and auto-complete predictions are rendered in a readable, muted gray (`#787878`), perfectly legible on dark backgrounds.

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl+` ` (Backtick) | Toggle bottom panel / terminal visibility |
| `Ctrl+C` / `Ctrl+V` | Copy and paste in terminal |
| `Tab` | Shell auto-completion |
