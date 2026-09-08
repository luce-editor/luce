---
id: diagnostics
title: Diagnostics & Error Detection
sidebar_label: Diagnostics & Problems
slug: /interface/diagnostics
---

# Diagnostics & Error Detection

Luce features a built-in **Diagnostic Engine** that inspects code in real-time and on save using native compilers and linters, highlighting issues directly within the code editor.

---

## How It Works

The engine executes background syntax-only compilation checks without blocking the UI:

- **C / C++**: Invokes `clang++ -fsyntax-only -Wall` (with automatic fallback to `g++` or MSVC `cl.exe /Zs`).
- **Python**: Invokes bytecode syntax check via `python -m py_compile`.
- **Rust**: Runs fast syntax checks via `rustc --error-format=short`.

Compiler diagnostic messages are parsed into structured problem items containing the file path, line number, column, severity, and error message.

---

## Editor Visualization

### 1. Squiggly Underlines
- Errors are highlighted with a distinct wavy red squiggly line (`~ ~ ~`) under the problematic code segment.
- Warnings are marked with a warm yellow squiggly underline.
- **Hover Tooltips**: Hovering the mouse cursor over underlined code reveals a floating tooltip containing the compiler's diagnostic message.

### 2. Problems Panel (Bottom Dock)
- The **Problems** tab in the bottom dock displays a project-wide overview of all detected errors and warnings.
- **Navigation**: Double-clicking any problem entry instantly opens the file and places the cursor right at the error's line and column.

---

## Triggering Diagnostics

1. **Automatically**: On every file save (`Ctrl+S`).
2. **Keyboard Shortcut**: **Ctrl+Shift+B** (runs check on active file).
3. **Command Palette**: `Diagnostics: Check Active File`.
