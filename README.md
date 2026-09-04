# Luce — A Modern Code Editor

**Luce** is a lightweight, GPU-accelerated code editor written in C++23 using [Dear ImGui](https://github.com/ocornut/imgui), [SDL2](https://www.libsdl.org/), and OpenGL 3.3. Inspired by Zed and VS Code, it delivers an ultra-fast, native desktop editing experience with rich SVG file icons, Live Markdown Preview, Emmet snippets, interactive terminal, and an extensible Lua 5.4 scripting plugin system.

![Status](https://img.shields.io/badge/status-active%20development-orange)
![C++](https://img.shields.io/badge/C%2B%2B-23-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

---

## Features

- **Modern Visuals & Theme Engine**:
  - Exact VS Code Dark Modern palette (`#181818` background, tailored syntax colors).
  - IBM Plex Sans UI typography & Lilex Code Monospace font.
  - Native SVG vector icon rasterizer engine with real file & folder icons.
  - GUI Zoom scaling (`Ctrl + +`, `Ctrl + -`, `Ctrl + 0`).
- **Syntax Highlighting**:
  - **C / C++** (C++23 keywords, preprocessor directives, include paths)
  - **HTML + CSS + JavaScript** (contextual lexer switching)
  - **Rust** (lifetimes, macros, nested block comments)
  - **Markdown** (headings, bold, italics, inline code, code blocks)
  - **CMake** (`CMakeLists.txt` and `*.cmake` scripts)
- **Live Markdown Preview**:
  - Side-by-side rich formatted preview with headings, bold text, lists, and code blocks (`Ctrl+Shift+M`).
- **Emmet HTML Abbreviations**:
  - Type `!`, `html:5`, `div`, `ul>li`, `table`, or `.class` and press `Tab` to expand boilerplate instantly.
- **Lua 5.4 Scripting Plugin System**:
  - Zero-compilation plugin model — write a `.lua` file and drop it into `plugins/` next to the executable.
  - Full editor API exposed to scripts: insert/delete text, register Command Palette commands, query cursor position and file path.
  - Isolated `lua_State` per plugin — one broken script cannot crash others.
  - Optional `on_tick(dt)` and `on_shutdown()` lifecycle callbacks.
- **Embedded Terminal & Process Engine**:
  - Integrated interactive terminal subprocess (`cmd.exe` / `powershell` / `bash`) with real-time I/O piping.
- **File Explorer & Workspace**:
  - File tree view with root workspace header, folder context actions (New File, New Folder, Rename, Delete).
  - One-click Explorer Refresh button.
  - Session state persistence in `session.json` across app restarts.
- **Command Palette & Quick Open**:
  - `Ctrl+Shift+P` for actions, `Ctrl+P` for fuzzy file search, `Ctrl+G` for Go to Line.
- **Multi-Cursor Editing**:
  - `Ctrl+Click` to place multiple carets, `Ctrl+D` to select next occurrence.

---

## Quick Start

### Requirements

- **CMake** 3.20+
- **C++23** compiler (MSVC 2022 v17.10+, GCC 13+, or Clang 16+)
- **OpenGL 3.3+** compatible GPU
- **Git** (for automatic FetchContent dependency resolution)

### Building on Windows

```powershell
# Clone the repository
git clone https://github.com/your-username/luce.git
cd luce

# Configure and build
cmake -B build
cmake --build build --config Release

# Run
.\build\Release\luce.exe
```

### Building on Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install -y build-essential cmake libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run
./build/luce
```

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New file |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save file |
| `Ctrl+W` | Close active tab |
| `Ctrl+P` | Quick Open (Search project files) |
| `Ctrl+Shift+P` | Command Palette |
| `Ctrl+Shift+M` | Toggle Markdown Live Preview |
| `Ctrl+G` | Go to line number |
| `Ctrl+F` | Find in file |
| `Ctrl+H` | Replace in file |
| `Ctrl+D` | Select next occurrence |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+/` | Toggle comment |
| `Ctrl+Tab` | Switch to next tab |
| `Ctrl+\`` | Toggle terminal |
| `Ctrl+=` / `Ctrl+-` | Zoom In / Zoom Out GUI |
| `Ctrl+0` | Reset Zoom to 100% |
| `Tab` | Expand Emmet abbreviation (HTML) / Indent |

---

## Documentation

Detailed documentation is available on the [Luce Documentation Site](docs-site/).

- [Getting Started](docs-site/docs/intro.md)
- [Creating a Lua Plugin](docs-site/docs/plugins/creating-a-plugin.md)
- [Lua Plugin API Reference](docs-site/docs/plugins/api-reference.md)
- [Architecture & Design Guidelines](.agents/AGENTS.md)

---

## License

This project is licensed under the MIT License - see the `LICENSE` file for details.
