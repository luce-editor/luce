---
id: settings
title: Preferences & Settings
sidebar_label: Settings
slug: /interface/settings
---

# Preferences & Settings

Luce provides a unified, transparent configuration system stored entirely in human-readable JSON format, alongside an independent native graphical settings window inspired by **Zed** and **VS Code**.

---

## Opening Settings

Luce offers two convenient ways to access your configuration:

1. **Native Settings Window (UI)**:
   - Shortcut: `Ctrl + ,`
   - Menu bar: **File -> Settings**
   - Command Palette: `Preferences: Open Settings (UI)`
   - Welcome Screen: **Open Settings** button
   - Opens as an **independent, native Windows OS window** with a modern dark caption bar, standard minimize/maximize/close controls, and multi-monitor positioning.
2. **Direct JSON Configuration**:
   - Shortcut: `Ctrl + Shift + ,`
   - Menu bar: **File -> Open Settings File**
   - Alternatively click the **`{ } Open settings.json`** button in the top-right corner of the Settings window.

---

## Native Window & Modern Controls

### Independent OS Window (Zed Style)
The Settings window runs in its own native OS platform window:
- **True Multi-Tasking**: Position the settings window on a secondary monitor or snap it side-by-side with Luce.
- **Dark Mode Titlebar**: Seamlessly styled with Windows 10/11 immersive dark mode and native window shadow.
- **Clutter-Free Workspace**: Editor tabs remain dedicated strictly to source code and document buffers.
- **Live Two-Way Sync**: Adjusting settings instantly updates the code editor in real time, and editing `settings.json` hot-reloads into the UI.

### Modern Pill-Shaped Toggle Switches
All boolean options use custom pill-shaped toggle switches with smooth animations (`ImLerp`) and hand hover cursors instead of standard checkboxes:
- **Left Column**: Feature title and wrapped explanatory description.
- **Right Column**: Tactile toggle switch cleanly pinned to the card's right edge.

---

## Storage & File Location

All user preferences are persisted in `settings.json`, located in Luce's executable directory (or user application directory on other platforms).

Any changes made in the graphical Settings UI are automatically and immediately saved to `settings.json`. Conversely, editing and saving `settings.json` directly reloads and applies your configuration on the fly.

### JSON Schema & Example

```json
{
  "editor": {
    "font_size": 16,
    "tab_size": 4,
    "use_spaces": true,
    "show_minimap": false,
    "show_line_numbers": true,
    "highlight_current_line": true,
    "zoom_with_mouse_wheel": true,
    "cursor_blinking": true
  },
  "ui": {
    "font_size": 15,
    "scale": 1.0,
    "theme": "VS Code Dark 2026"
  },
  "general": {
    "auto_save": "off",
    "show_welcome_on_startup": true
  }
}
```

---

## Settings Categories

The Settings UI organizes preferences into focused, clean categories matching the visual hierarchy of the File Explorer and Source Control sidebars:

### 1. Text Editor Settings
- **Editor: Font Size**: Adjust the code buffer font size (10 to 36 px) using the slider, `-` / `+` step buttons, or the `Reset (15px)` button with a live syntax preview box.
- **Editor: Mouse Wheel Zoom**: Toggle switch for smooth font scaling when holding `Ctrl` and scrolling the mouse wheel.
- **Editor: Tab Size**: Quickly switch indentation widths between 2, 4, or 8 spaces.
- **Editor: Insert Spaces**: Toggle switch between soft spaces and hard tab characters (`\t`).
- **Editor: Minimap**: Toggle switch to enable or disable the code thumbnail overview on the right edge (`Ctrl+M`).
- **Editor: Line Numbers**: Toggle switch for gutter line numbers display.
- **Editor: Highlight Active Line**: Toggle switch for highlighting the background of the active cursor line.
- **Editor: Cursor Blinking**: Toggle switch for smooth 1 Hz caret blinking during editing pauses.

### 2. Appearance & Themes
- **UI: Text Font Size**: Customize the text size of menus, sidebars, tabs, and status bar (10 to 26 px) with `-`, `+`, and `Reset (15px)` buttons.
- **Window: Interface Zoom / Scale**: Adjust the global display scale factor (75% to 200%) with quick 100%, 125%, 150%, and `Reset (100%)` presets.
- **Workbench: Color Theme Gallery**: Interactive visual cards with 6-color palette swatches (including syntax tokens and bottom status bar preview). Clicking any card activates the theme instantly.
- **Workbench: File & Folder Icons**: Manage custom SVG and PNG icons. Quickly open the `icons/` directory, edit `icons.json` mapping rules, and reload icons on the fly without restarting.

### 3. General Preferences
- **Files: Auto Save**: Configure automatic saving behavior:
  - `Off (manual save)`
  - `On Window Focus Lost`
  - `After 1s Delay`
- **Startup: Show Welcome Page**: Toggle switch to show the Welcome page automatically on launch when no files are open.
- **Configuration: Storage File**: View current `settings.json` path, with one-click buttons to **Open in Editor** or **Reveal in File Explorer**.

### 4. Keyboard Shortcuts Reference
A filterable, searchable list of all built-in keybindings organized by shortcut, command name, and category.

### 5. Plugins & Extensions
Browse installed extensions, view directories, and open the plugins folder.

### 6. About Luce
Live memory consumption diagnostics (working set RAM in MB) and core architecture breakdown.
