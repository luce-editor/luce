---
id: architecture
title: Core Architecture
sidebar_label: Architecture
slug: /architecture
---

# Luce Core Architecture

Luce is designed around strict separation of concerns, data-oriented memory efficiency, and immediate-mode UI rendering.

```
┌────────────────────────────────────────────────────────────────┐
│                           Main Loop                            │
│                 (SDL2 Event Pump + OpenGL 3.3)                 │
└───────────────────────────────┬────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│                         App Shell (UI)                         │
│   ├── Horizontal Activity Bar (File Explorer & Plugins)        │
│   ├── TabBar & Docking Workspace                               │
│   ├── Command Palette & Search Overlay                         │
│   └── Embedded Terminal Panel                                  │
└───────────────────────────────┬────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌────────────────┐      ┌────────────────┐      ┌────────────────┐
│  Editor Engine │      │ Syntax Engine  │      │ Plugin Manager │
│ ├── TextBuffer │      │ ├── Lexer C++  │      │ ├── Lua 5.4 VM  │
│ ├── Undo/Redo  │      │ ├── Lexer Rust │      │ ├── lua_State[] │
│ └── VirtualPos │      │ └── Line Cache │      │ └── luce.* API  │
└────────────────┘      └────────────────┘      └────────────────┘
```

---

## 1. Separation of Layers

- **`src/editor/`**: Pure text document storage and editing logic. `TextBuffer` manages line storage, piece-table like mutations, and undo/redo stacks. `EditorView` handles line wrapping, virtual scrolling rendering, selection ranges, and cursor blink timers.
- **`src/syntax/`**: Stateful tokenization engine. `Lexer` provides an abstract base class with single-line tokenization that accepts and returns an opaque `LexerState`. `SyntaxHighlighter` caches token spans per line and invalidates lines incrementally upon editing.
- **`src/ui/`**: Immediate-mode Dear ImGui widgets, custom styles, Tab Bar manager, File Explorer tree, Command Palette, and Theme Manager.
- **`src/plugin/`**: Lua 5.4 scripting engine. `PluginManager` scans the `plugins/` folder and loads each `.lua` file into its own isolated `lua_State`. The `luce.*` table exposes the full editor API to scripts.
- **`src/platform.*`**: Operating system abstraction layer for native file dialogs, process pipes (`platform::Process`), and executable discovery.

---

## 2. Virtual Scrolling & Performance

In conventional naive UI architectures, iterating through a 50,000-line file every frame causes devastating lag.

Luce solves this using **Virtual Scrolling**:
1. At the start of the frame, `EditorView` queries ImGui's scroll position `ImGui::GetScrollY()` and visible viewport height.
2. It calculates the visible line window:
   ```
   StartLine = floor(ScrollY / LineHeight)
   EndLine   = StartLine + ceil(ViewportHeight / LineHeight) + 2
   ```
3. Only the lines within `[StartLine, EndLine]` are measured, tokenized, and rendered via `ImDrawList`.
4. The remaining invisible document lines consume zero CPU and GPU cycles per frame.

---

## 3. Stateful Incremental Lexing

Languages with multi-line constructs (e.g. block comments `/* ... */`, multi-line raw strings `R"(...)"`, multiline templates) cannot be parsed purely statelessly per line.

Luce's `Lexer` interface requires each lexer to implement:

```cpp
virtual LexerState TokenizeLine(
    const std::string& line, 
    LexerState initial_state, 
    std::vector<Token>& out_tokens
) = 0;
```

`SyntaxHighlighter` tracks the `LexerState` returned at the end of each line:
- If line `N` is edited, only lines from `N` onward are re-lexed until the resulting `LexerState` matches the previously cached state for subsequent lines.
- Lines that were unaffected are retrieved instantly from the per-line token cache.
