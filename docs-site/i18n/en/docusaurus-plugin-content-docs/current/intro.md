---
id: intro
title: Introduction & Quick Start
sidebar_label: Introduction
slug: /intro
---

# Welcome to Luce Editor

**Luce** is a blazing-fast, lightweight, and modern code editor written in **C++23** using **Dear ImGui** (docking branch), **SDL2**, and **OpenGL 3.3**.

Designed with minimalism, extreme responsiveness, and modular extensibility in mind, Luce combines the visual polish of modern IDEs with the raw speed and low memory footprint of a native native desktop application.

![Luce Architecture Overview](@site/static/img/luce-logo.png)

---

## Key Features

- **Native C++23 Performance:** Minimal latency, sub-second cold boot, low RAM footprint.
- **Rich Theme Engine:** Built-in VS Code Dark Modern, Catppuccin Mocha, One Dark, and Nord themes, plus custom CSS-like theme files with **Live Hot Reload**.
- **Lua 5.4 Plugin System:** Scriptable editor extensions — just drop a `.lua` file into the `plugins/` folder, no compilation or setup required.
- **Docking Window Manager:** Powered by Dear ImGui Docking, allowing full flexibility to arrange Editor, Sidebar, and bottom dock panels.
- **Embedded Terminal:** Multi-session VT100 terminal with tab management, PowerShell (`pwsh.exe`) / Bash, automatic project folder sync, and TrueColor support.
- **Diagnostics & Problems Panel:** Dedicated Problems tab with clickable error navigation — double-clicking jumps directly to the source code line.
- **Command Palette & Quick Open:** Full fuzzy-style command palette (`Ctrl+Shift+P`), file jumping (`Ctrl+P`), and line jumping (`:line_number`).
- **Virtual Scrolling & Incremental Highlighting:** Documents with 100,000+ lines render smoothly at 60+ FPS using line-by-line token caching and stateful hand-written lexers.

---

## Building from Source

### Prerequisites

- **CMake 3.20** or newer
- **C++23 compatible compiler**:
  - Windows: MSVC v143 (Visual Studio 2022 17.4+) or Clang 16+
  - Linux: GCC 13+ or Clang 16+
  - macOS: Apple Clang 15+ or LLVM Clang 16+
- **OpenGL 3.3+** graphics drivers

All dependencies (SDL2, Dear ImGui Docking branch, nlohmann/json) are fetched automatically via CMake's `FetchContent`.

### Build Steps (Windows / Visual Studio)

```powershell
# Clone the repository
git clone https://github.com/luce-editor/luce.git
cd luce

# Configure CMake with Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build executable and plugins
cmake --build build --config Release

# Run Luce
.\build\Release\luce.exe
```

### Build Steps (Linux / macOS)

```bash
# Configure & build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)

# Run Luce
./build/luce
```

---

## Default Keyboard Shortcuts

| Shortcut | Action | Description |
| :--- | :--- | :--- |
| `Ctrl+Shift+P` | **Command Palette** | Open palette to execute any registered command |
| `Ctrl+P` | **Quick Open** | Rapidly search and open project files |
| `Ctrl+G` | **Go to Line** | Jump to a specific line number (`:line`) |
| `Ctrl+O` | **Open File** | Native OS file chooser dialog |
| `Ctrl+Shift+O` | **Open Folder** | Open project directory in File Explorer |
| `Ctrl+S` | **Save File** | Save active document buffer |
| `Ctrl+W` | **Close Tab** | Close current editor tab |
| `Ctrl+Z` | **Undo** | Undo text buffer modification |
| `Ctrl+Y` / `Ctrl+Shift+Z` | **Redo** | Redo previously undone action |
| `Ctrl+=` / `Ctrl++` | **Zoom In** | Increase editor font scale |
| `Ctrl+-` | **Zoom Out** | Decrease editor font scale |
| `Ctrl+0` | **Reset Zoom** | Reset font size to default |
| `Ctrl+` ` | **Toggle Terminal** | Show / hide bottom embedded terminal |

---

## License

Luce is licensed under the open-source **MIT License**.
