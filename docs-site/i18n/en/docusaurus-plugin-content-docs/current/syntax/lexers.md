---
id: lexers
title: Syntax Highlighting & Lexers
sidebar_label: Lexers & Syntax
slug: /syntax/lexers
---

# Syntax Highlighting & Lexers

Luce uses hand-written deterministic state-machine lexers for maximum speed, predictable tokenization, and zero third-party grammar dependencies.

---

## Supported Languages

Luce includes built-in lexers for:
- **C & C++** (`.c`, `.cpp`, `.cc`, `.cxx`, `.h`, `.hpp`):
  - Complete modern keyword coverage from C++11 through C++23 and upcoming C++26 according to cppreference.com (e.g. modules `import`/`export`, contracts `contract_assert`/`pre`/`post`, transactional memory `atomic_commit`, reflection `reflexpr`, and more).
  - Dedicated preprocessor directive tokenization, including intelligent recognition of `#pragma once` and header paths `#include <...>`.
- **Python** (`.py`, `.pyw`, `.pyi`):
  - Comprehensive keyword coverage (`def`, `class`, `async`, `await`, `match`, `case`, etc.) plus built-in types and functions (`str`, `int`, `list`, `dict`, `print`, `len`, etc.).
  - Multi-line docstrings with triple quotes `"""` and `'''`, f-strings, decorators `@`, and comments `#`.
  - Automatic indentation (tab) when pressing `Enter` after a colon `:`.
- **Rust** (`.rs`)
- **Web Technologies**: HTML, CSS, JavaScript (`.html`, `.htm`, `.css`, `.js`)
- **CMake** (`CMakeLists.txt`, `.cmake`)
- **Markdown** (`.md`) with real-time preview (`Ctrl+Shift+M`)

---

## Adding a New Language Lexer

1. Create `src/syntax/lexer_<lang>.h` and `src/syntax/lexer_<lang>.cpp`.
2. Inherit from `luce::Lexer` and implement `TokenizeLine()`.
3. Register the lexer in the `SyntaxHighlighter` constructor in `src/syntax/syntax_highlighter.cpp`.
