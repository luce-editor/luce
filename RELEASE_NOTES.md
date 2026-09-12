# Luce v0.1.0 — Initial Release 🎉

Welcome to the very first public release of **Luce**, a lightweight, GPU-accelerated code editor built from scratch in **C++23** using Dear ImGui, SDL2, and OpenGL 3.3.

---

## ✨ Key Highlights

- ⚡ **GPU-Accelerated & Ultra-Fast**: Native C++23 engine with Dear ImGui docking branch, virtual scrolling, and high refresh rates.
- 🎨 **Modern Dark Aesthetics**: Crisp UI featuring native vector SVG file/folder icons, curated color schemes, and IBM Plex Sans + Lilex typography.
- 🔀 **Drag & Drop & Split View**: Effortless side-by-side editing (`Ctrl+\`), smooth tab reordering, and docking zones across panes.
- 📝 **Live Markdown Preview**: Instant formatted preview with headings, lists, code blocks, and real-time syncing (`Ctrl+Shift+M`).
- 🌿 **Git Gutter & Visual Diff Viewer**: Track changes directly in the line gutter (added, modified, deleted) and inspect differences with the built-in syntax-highlighted diff viewer.
- 🔍 **Command Palette & Fast Navigation**: Fuzzy file finder (`Ctrl+P`), command palette (`Ctrl+Shift+P`), and project-wide text search (`Ctrl+Shift+F`).
- 💻 **Embedded Terminal**: Multi-tab VT100 terminal supporting PowerShell, CMD, or Bash (`Ctrl+~`).
- 🧩 **Lua 5.4 Plugin Engine**: Extend Luce with custom commands, editor event hooks (`before_save`, `text_changed`), linters, and autocomplete providers.
- 🔤 **Syntax Highlighting & Emmet**: Deterministic lexers for C/C++, Rust, HTML/CSS/JS, Markdown, CMake, plus HTML Emmet expansion (`Tab`).

---

## 📦 Downloads & Installation

### Windows

- **`Luce-Setup-v0.1.0-x64.exe` (Recommended Installer)**
  - Installs cleanly per-user (no administrator privileges required).
  - Integrates *"Open with Luce"* into the Windows Explorer context menu.
  - Adds `luce` to your system `PATH` (launch anywhere using `luce .`).
  - Creates Start Menu and Desktop shortcuts.

- **`Luce-v0.1.0-win64-portable.zip` (Portable Archive)**
  - Fully portable — extract anywhere and run `luce.exe` without installation.

---

## 📚 Documentation & Links

- 🌐 **Documentation Website**: [luce-editor.github.io/luce](https://luce-editor.github.io/luce/)
- ⌨️ **Keyboard Shortcuts Guide**: [Shortcuts Reference](https://luce-editor.github.io/luce/docs/interface/keyboard-shortcuts)
- 🔌 **Plugin Development**: [Lua API Architecture](https://luce-editor.github.io/luce/docs/plugins/architecture)