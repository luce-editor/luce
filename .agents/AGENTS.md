# Luce — Agent Guidelines

## Project Overview

Luce is a code editor written in C++23 using Dear ImGui (docking branch)
with SDL2 and OpenGL3. The project is structured as a single CMake target
with all dependencies fetched via FetchContent.

## Coding Conventions

- **Language**: All comments, variable names, class names, and documentation
  must be in **English**.
- **Standard**: C++23. Use modern features freely (ranges, `std::expected`,
  structured bindings, `std::format` where supported, etc.).
- **Namespace**: All code lives inside `namespace luce {}`. Platform
  helpers are in `namespace luce::platform {}`.
- **Headers**: Use `#pragma once`. Header/implementation split for all
  non-trivial classes.
- **Comments**: Document every non-obvious function with a `///` doc comment
  explaining its purpose, parameters, and return value. Do not comment
  self-explanatory functions like getters/setters or `main()`.
- **Error handling**: Return `bool` or `std::optional` — no exceptions.
  Exceptions must never cross DLL boundaries.
- **Memory**: Prefer `std::unique_ptr` for owned resources. Raw pointers
  only for non-owning references. No manual `new`/`delete` outside RAII.

## Architecture Principles

1. **Separation of concerns**: UI rendering (`ui/`), text data
   (`editor/`), syntax analysis (`syntax/`), and OS interaction
   (`platform.*`) are distinct layers. Each can be modified independently.

2. **Plugin-ready design**: The app is designed so that a future plugin
   manager can be added without restructuring. The `LuceEditorAPI` struct
   in `include/luce/plugin_api.h` defines the stable C ABI.

3. **Virtual scrolling**: The editor only renders visible lines. Token
   caches are invalidated per-line. Never iterate the entire document
   in a per-frame path.

4. **Lexer architecture**: Lexers are hand-written state machines (not
   regex). They operate on one line at a time, receiving and returning a
   `LexerState` for multi-line constructs. This makes them fast and
   deterministic.

## How to Add a New Language Lexer

1. Create `src/syntax/lexer_<lang>.h` and `src/syntax/lexer_<lang>.cpp`.
2. Inherit from `luce::Lexer` and implement:
   - `TokenizeLine()` — tokenise one line, return the state for the next.
   - `GetLanguageName()` — e.g. `"Python"`.
   - `GetExtensions()` — e.g. `{".py", ".pyw"}`.
3. Register the lexer in `SyntaxHighlighter::SyntaxHighlighter()`
   constructor (`src/syntax/syntax_highlighter.cpp`).
4. Map language-specific concepts to existing `TokenType` values. Only
   add a new `TokenType` if no existing one fits.

## How to Add a New Theme

1. Open `src/ui/theme.cpp`.
2. Create a new function `MakeMyTheme()` returning a `Theme` struct.
3. Add it to the `ThemeManager` constructor.
4. Use the `Hex(0xRRGGBB)` helper for colour definitions.

## How to Add a New Command

1. In `App::RegisterCommands()` (`src/ui/app.cpp`), call:
   ```cpp
   command_palette_.RegisterCommand({
       "namespace.command_id",
       "Human-Readable Name",
       "Ctrl+Key",          // optional shortcut hint
       [this]() { /* action */ }
   });
   ```
2. If the command needs a global keybinding, add it to the shortcut block
   at the end of `App::Render()`.

## File Layout

```
src/
├── main.cpp              — SDL2/OpenGL3 init, main loop
├── platform.h/cpp        — OS abstraction (MUST keep cross-platform)
├── editor/               — Core editing data structures
│   ├── cursor.h          — Position + selection (header-only)
│   ├── text_buffer.*     — Document storage + undo/redo
│   └── editor_view.*     — ImGui editor widget
├── syntax/               — Tokenisation and highlighting
│   ├── lexer.h           — Abstract interface + TokenType enum
│   ├── lexer_cpp.*       — C/C++ lexer
│   ├── lexer_web.*       — HTML/CSS/JS lexer
│   ├── lexer_rust.*      — Rust lexer
│   └── syntax_highlighter.* — Lexer registry + per-line cache
└── ui/                   — ImGui UI components
    ├── app.*             — Main application shell
    ├── tab_bar.*         — Tab management
    ├── file_explorer.*   — Project tree sidebar
    ├── command_palette.* — Ctrl+Shift+P overlay
    ├── terminal_panel.*  — Embedded terminal
    └── theme.*           — Colour themes
```

## Build & Dependencies

- CMake 3.20+, C++23 compiler.
- FetchContent pulls SDL2 and ImGui (docking branch) automatically.
- OpenGL comes from the system.
- JetBrains Mono font is downloaded at configure time.

## Testing

Currently there are no automated unit tests. Verification is manual:
build the project, run `luce`, and open various source files to check
syntax highlighting, editing, and UI features.

## Future Work

- **Plugin loading** — Implement `PluginManager` that scans `plugins/`
  for .dll/.so files and loads them via the API in `plugin_api.h`.
- **Settings persistence** — Save/load user preferences (font, theme,
  tab size) to a JSON config file.
- **Tree-sitter integration** — Replace hand-written lexers with
  tree-sitter grammars for more accurate highlighting.
- **LSP client** — Integrate Language Server Protocol for autocomplete,
  diagnostics, and go-to-definition.
- **Vulkan backend** — Alternative rendering backend for future-proofing.
- **Minimap** — Thumbnail code overview panel.
