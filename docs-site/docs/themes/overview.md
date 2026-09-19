---
id: overview
title: Themes & Color Palettes
sidebar_label: Overview
slug: /themes/overview
---

# Themes & Color Palettes

Luce features a comprehensive theming engine capable of skinning all editor elements—including syntax highlight tokens, UI chrome, gutter line numbers, tab headers, terminal panes, and background clear colors.

---

## Built-in Themes

## Built-in & Custom Themes

Luce ships with 4 built-in themes plus dynamic CSS themes in the `themes/` directory:

1. **VS Code Dark 2026** *(Default)*: Classic, subtle dark slate palette inspired by Visual Studio Code (`#181818`).
2. **Catppuccin Mocha**: Warm, pastel soothing palette with signature mauve status bar text.
3. **One Dark**: Balanced, iconic dark theme with Atom sky-blue accents.
4. **Nord**: Clean, arctic cool theme with Polar Night slate and frost cyan text.
5. **Dracula** (`themes/dracula.css`): Vibrant slate purple status bar and high-contrast tokens.
6. **Cyberpunk 2077** (`themes/cyberpunk.css`): Dark neon violet with glowing cyan accents.

---

## Dynamic Status Bar Theming

Every theme fully customizes the bottom status bar:
- `statusbar_bg`: The background fill of the status bar.
- `statusbar_fg`: The foreground color for Git branch, language mode, cursor coordinates, encoding, indentation, and version.
- A subtle 1px border line matching `statusbar_fg` creates clean visual separation between the editor/terminal and status bar.

---

## Switching Themes

You can switch the active theme anytime using:

1. **Settings Gallery (`Ctrl+,`)**:
   - Go to **File -> Settings -> Appearance & Themes**.
   - Browse interactive theme cards showing syntax tokens and status bar color swatches.
2. **Command Palette (`Ctrl+Shift+P`)**:
   - Type `Theme:` and press `Enter` on your choice.
3. **Menu Bar**:
   - **View -> Theme -> Choose from the list**.

All interface elements, status bar, and syntax highlighting update immediately in real-time and persist to `settings.json`.
