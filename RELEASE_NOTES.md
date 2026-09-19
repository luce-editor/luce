# Luce v0.1.5 — Release Notes

Welcome to **Luce v0.1.5**! This release brings significant UI/UX enhancements, independent native windowing for settings, custom iconography, major startup performance optimizations, and expanded language syntax support.

---

## 🚀 Key Highlights in v0.1.5

### 1. Native Windows Settings Window (Zed Style)
- **Independent OS Window**: Pressing `Ctrl+,` (or **File > Settings**) now opens Preferences in an independent, native Windows OS window with a modern dark caption bar and standard minimize, maximize, and close `[X]` buttons.
- **Multi-Monitor & Snapping**: You can now move settings to a secondary monitor or snap it side-by-side with your code editor.
- **Live Two-Way Sync**: Changing any setting instantly reflects live in the code editor, and changes to `settings.json` hot-reload into the UI seamlessly.
- **Clean Editor Tabs**: Editor tabs now strictly represent your open documents and files without settings cluttering your tab bar.

### 2. Modern Pill-Shaped Toggle Switches
- **Zed-Style Sliders**: Replaced traditional checkboxes across the Settings UI and Welcome Screen with modern capsule-shaped toggle switches.
- **Tactile Micro-Animations**: Smooth sliding knob transitions (`ImLerp`) with hand pointer hover feedback.
- **Master-Detail Layout**: Setting titles and descriptions are aligned on the left with automatic word wrap, while switches are neatly pinned to the right edge.

### 3. Blazing Fast Startup (< 100 ms)
- **Instant Window Presentation**: Windows are created hidden and presented only after the first frame has been fully painted and swapped to the GPU. Zero black screen flash and no spinning wait cursor.
- **Asynchronous File Scanning & Directory Pruning**: Recursive project scanning now prunes build artifacts, `.git`, and `node_modules` folders using `disable_recursion_pending()` in a background worker thread. File scanning time dropped from **2,500 ms to < 2 ms** on large codebases.
- **Optimized FreeType Atlas Baking**: Reduced emoji codepoint ranges to target common pictographs and emoticons, cutting font rasterization overhead on launch.

### 4. Custom Icon Manager & `icons.json`
- **Extensible Icon Theming**: Support for custom SVG and PNG icons mapped via `icons/icons.json` for specific folder names, file extensions, and exact filenames.
- **Hot Reloading**: Reload icon sets on the fly without restarting the editor using `Icons: Reload Icons` in the Command Palette.
- **File Explorer Polish**: Added hand pointer cursor when hovering files/folders and theme-driven selection highlights.

### 5. Welcome Screen & Getting Started
- **Distraction-Free Dashboard**: Beautiful Zed-inspired Welcome page featuring quick actions (New File, Open Folder, Clone Repo), recent workspace history, and configuration shortcuts.
- **Startup Customization**: Configurable toggle switch to automatically show the Welcome screen when starting Luce with no open tabs.

### 6. Expanded Lexers & Plugin Architecture
- **New Syntax Lexers**: Hand-written, fast state-machine lexers for **JSON** and **Lua** with full token classification and theme integration.
- **Modular Plugin Directory**: Each Lua plugin now lives in its own directory (`plugins/<name>/init.lua`) with optional metadata, custom icons, and snippet providers.
- **Plugin Management**: Inspect installed extensions, open plugin directories, and safely delete plugins directly through the UI with modal confirmation.

---

## 📦 Downloads & Installation

### Windows (x64)

- **`Luce-Setup-v0.1.5-x64.exe` (Recommended Installer)**
  - Installs per-user without requiring administrator privileges.
  - Adds *"Open with Luce"* context menu entry to Windows Explorer.
  - Registers `luce` in system `PATH` (launch anywhere from terminal: `luce .`).
  - Creates Desktop and Start Menu shortcuts.

- **`Luce-v0.1.5-win64-portable.zip` (Portable Archive)**
  - Fully portable — extract anywhere and run `luce.exe` without installation.

---

## 📖 Documentation & Links

- **Documentation Website**: [luce-editor.github.io/luce](https://luce-editor.github.io/luce/)
- **Preferences & Settings Guide**: [Settings Reference](https://luce-editor.github.io/luce/docs/interface/settings)
- **Keyboard Shortcuts Reference**: [Shortcuts Guide](https://luce-editor.github.io/luce/docs/interface/keyboard-shortcuts)
- **Lua Plugin Architecture**: [Plugin Documentation](https://luce-editor.github.io/luce/docs/plugins/architecture)