---
id: lexers
title: Podświetlanie Składni i Leksery
sidebar_label: Leksery i Składnia
slug: /syntax/lexers
---

# Podświetlanie Składni i Leksery

Luce korzysta z ręcznie pisanych, deterministycznych automatów skończonych (maszyn stanowych) w celu uzyskania maksymalnej szybkości i zerowej liczby zewnętrznych zależności gramatycznych.

---

## Obsługiwane Języki

- **C & C++** (`.c`, `.cpp`, `.cc`, `.cxx`, `.h`, `.hpp`):
  - Kompletny zestaw słów kluczowych C++11/C++20/C++23 oraz nadchodzącego C++26 zgodnie ze specyfikacją cppreference.com (m.in. moduły `import`/`export`, kontrakty `contract_assert`/`pre`/`post`, transakcje `atomic_commit`, refleksje `reflexpr` i wiele innych).
  - Dedykowane podświetlanie dyrektyw preprocesora, w tym inteligentne rozpoznawanie `#pragma once` i ścieżek nagłówków `#include <...>`.
- **Python** (`.py`, `.pyw`, `.pyi`):
  - Pełen zestaw słów kluczowych (`def`, `class`, `async`, `await`, `match`, `case` itp.), wbudowane typy i funkcje (`str`, `int`, `list`, `dict`, `print`, `len` itp.).
  - Wielowierszowe docstringi potrójnych cudzysłowów `"""` i `'''`, f-stringi, dekoratory `@` oraz komentarze `#`.
  - Inteligentne automatyczne wcięcie (tab) po dwukropku `:` przy wciśnięciu klawisza `Enter`.
- **Rust** (`.rs`)
- **JSON** (`.json`):
  - Wydajna maszyna stanów rozpoznająca klucze/właściwości, wartości tekstowe, literały liczbowe, wartości logiczne/null (`true`, `false`, `null`), interpunkcję oraz komentarze (`//`, `/* */`).
  - Zintegrowane rozszerzenie Lua `json_intellisense` dostarczające inteligentne podpowiedzi kluczy pliku `settings.json`, nazw motywów i wartości JSON.
- **Technologie Webowe**: HTML, CSS, JavaScript (`.html`, `.htm`, `.css`, `.js`)
- **Lua** (`.lua`):
  - Dedykowany lekser ze wsparciem dla słów kluczowych Lua 5.1–5.4 (`local`, `function`, `then`, `end` itp.), łańcuchów wielowierszowych (`[[ ... ]]`), komentarzy blokowych (`--[[ ... ]]`) i operatorów.
- **CMake** (`CMakeLists.txt`, `.cmake`)
- **Markdown** (`.md`) z podglądem na żywo (`Ctrl+Shift+M`)

---

## Jak Dodać Nowy Język

1. Utwórz `src/syntax/lexer_nazwa.h` oraz `.cpp`.
2. Dziedzicz po `luce::Lexer` i zaimplementuj `TokenizeLine()`.
3. Zarejestruj lekser w konstruktorze `SyntaxHighlighter` w `src/syntax/syntax_highlighter.cpp`.
