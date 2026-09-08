---
id: architecture
title: Architektura Systemu Pluginów
sidebar_label: Architektura
slug: /plugins/architecture
---

# Architektura Systemu Pluginów

Luce używa **Lua 5.4** jako silnika skryptowego dla pluginów. Każdy plugin to zwykły plik `.lua` — bez kompilacji, bez CMake, bez konfiguracji.

---

## Jak działa ładowanie pluginów

Przy każdym uruchomieniu `PluginManager` skanuje katalog `plugins/` leżący obok `luce.exe`. Każdy znaleziony plik `.lua` jest ładowany w osobnym, izolowanym środowisku Lua.

```
build/Release/
├── luce.exe
└── plugins/
    ├── uppercase.lua     ← automatycznie załadowany
    ├── timestamp.lua     ← automatycznie załadowany
    └── moj_plugin.lua    ← automatycznie załadowany
```

---

## Izolacja środowisk

Każdy plugin otrzymuje **własny `lua_State`** (izolowane środowisko VM). Oznacza to:

- Błąd lub crash jednego skryptu nie wpływa na pozostałe.
- Pluginy nie widzą nawzajem swoich zmiennych globalnych.
- Każdy plugin ma własną kopię standardowej biblioteki Lua (`os`, `string`, `math`, `table` itp.).

---

## Rejestracja API

Po załadowaniu pliku, Luce automatycznie udostępnia globalną tabelę `luce` zawierającą pełne API edytora.
Skrypt może wtedy wywołać `luce.register_command(...)` czy `luce.insert_text(...)` bez żadnych importów.

```
lua_State (plugin A)          lua_State (plugin B)
┌──────────────────────┐      ┌──────────────────────┐
│  luce.* API (C++)    │      │  luce.* API (C++)     │
│  on_tick / on_shutdown│      │  on_tick / on_shutdown│
│  zmienne globalne A  │      │  zmienne globalne B   │
└──────────────────────┘      └──────────────────────┘
         │                              │
         └──────────┬───────────────────┘
                    │
             PluginManager (C++)
                    │
            CommandPalette / TabBar / ...
```

---

## Cykl życia pluginu

| Etap             | Co się dzieje                                                    |
|------------------|------------------------------------------------------------------|
| **Init**         | `luaL_dofile()` — plik `.lua` jest wykonywany od góry do dołu   |
| **Rejestracja**  | Skrypt woła `luce.register_command(...)` podczas wykonania       |
| **Tick**         | Luce woła globalną funkcję `on_tick(dt)` każdą klatkę (jeśli zdefiniowana) |
| **Shutdown**     | Luce woła `on_shutdown()` przy zamykaniu, potem zamyka `lua_State` |

---

## Instalacja pluginu

1. Napisz plik `.lua` implementujący plugin.
2. Skopiuj go do folderu `plugins/` obok `luce.exe`.
3. Zrestartuj Luce — plugin zostanie automatycznie załadowany.

> Kliknij **"Open Plugins Folder"** w zakładce Plugins w sidebarze, aby szybko otworzyć ten folder w Eksploratorze Windows.
