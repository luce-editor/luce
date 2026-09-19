---
sidebar_position: 1
title: Welcome Screen
description: Overview of Luce's Zed-style Welcome / Getting Started screen
---

# Welcome Screen

Luce features a native, distraction-free **Welcome Screen** (Getting Started) inspired by modern high-performance editors like **Zed**. It appears automatically when you launch the editor without open tabs, or whenever invoked via **Help > Welcome (Getting Started)** or the Command Palette (`help.welcome`).

The UI is styled to blend harmoniously with Luce's File Explorer, Source Control, and Plugin sidebars, featuring crisp typography, uppercase category markers, subtle separators, and SVG icons.

---

## Layout & Structure

The Welcome Screen presents a balanced, centered interface divided into key functional areas:

### 1. Header
- **Title**: Welcome to Luce
- **Subtitle**: Fast, lightweight C++23 code editor with native UI

### 2. Get Started
Quick actions to start coding immediately:
- **New File** (`Ctrl+N`): Creates a fresh untitled document.
- **Open Folder...** (`Ctrl+O`): Invokes the native folder picker and sets the active workspace root.
- **Clone Repository...** (`Git`): Opens the built-in modal to clone a Git repository directly into a new workspace folder.
- **Open Command Palette** (`Ctrl+Shift+P`): Opens the fuzzy command launcher.

### 3. Recent Workspaces
Displays your most recently opened project folders:
- Shows the workspace project name alongside its path.
- Clicking any project folder instantly opens that workspace and scans project files.

### 4. Configuration
One-click access to customization:
- **Open Settings** (`Ctrl+,`): Opens the independent native Settings window.
- **Keyboard Shortcuts** (`Keys`): Jumps straight to the Keybindings section in Settings.
- **Explore Extensions** (`Plugins`): Switches the sidebar to the Plugins view to explore and manage installed Lua plugins.
- **Color Themes** (`Theme`): Opens the Command Palette filtered by `Theme:` to preview and switch color palettes in real time.

### 5. Help & Resources
Direct links to documentation and community resources:
- **Documentation**: Opens the official Luce documentation site.
- **GitHub Repository**: Navigates to the source repository.
- **Report an Issue**: Quick link to submit bug reports and feedback.

### 6. Footer Preferences
At the bottom of the page, a tactile toggle switch controls startup behavior:
- **Show Welcome page on startup**: When enabled, the Welcome screen opens automatically if Luce starts with no project files open.

---

## Configuration

The Welcome screen behavior can be configured either via the checkbox on the Welcome page itself, through the **General** category in the Settings UI, or directly in `settings.json`:

```json
{
    "ui": {
        "show_welcome_on_startup": true
    },
    "recent_projects": [
        "c:/Users/Szymon/Desktop/luce",
        "c:/Projects/my-app"
    ]
}
```
