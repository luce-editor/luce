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

Przy każdym uruchomieniu (lub po kliknięciu ikony odświeżenia `↻` w panelu bocznym) `PluginManager` skanuje katalog `plugins/` leżący obok `luce.exe`. 

Luce wspiera dwa formaty wtyczek:
1. **Pakiety katalogowe (zalecane)**: folder zawierający plik `init.lua`, opcjonalny `README.md` z dokumentacją, plik `manifest.json` oraz ikony lub moduły pomocnicze.
2. **Pojedyncze skrypty**: samodzielne pliki `.lua`.

```
build/Release/
├── luce.exe
└── plugins/
    ├── cpp_snippets/           ← pakiet katalogowy
    │   ├── init.lua            ← główny punkt wejścia
    │   └── README.md           ← opis i dokumentacja wtyczki
    ├── json_intellisense/      ← pakiet katalogowy
    │   ├── init.lua
    │   └── README.md
    └── uppercase.lua           ← pojedynczy skrypt Lua
```

Każda wtyczka jest automatycznie ładowana w osobnym, bezpiecznym i izolowanym środowisku Lua.

---

## Widok Pluginów w Sidebarze (Plugins View)

Zarządzanie zainstalowanymi rozszerzeniami odbywa się w dedykowanym panelu w pasku bocznym (Sidebar):

- **Ikona w Activity Bar**: Trzecia ikona w górnym poziomym pasku aktywności (obok Explorera i Gita) przełącza panel na widok wtyczek.
- **Wyszukiwarka (`Search plugins...`)**: Filtrowanie listy wtyczek w czasie rzeczywistym po nazwie, opisie lub autorze.
- **Przeładowanie na żywo (`↻` / `Reload Plugins`)**: Przycisk na pasku nagłówka natychmiast re-skanuje folder `plugins/` i przeładowuje wszystkie wtyczki bez konieczności restartowania edytora.
- **Elementy listy wtyczek**:
  - Ikona wtyczki (dedykowana lub systemowa ikona SVG wtyczek).
  - Nazwa, wersja, autor oraz krótki opis.
  - **Przełącznik checkbox**: Szybkie włączanie lub wyłączanie wtyczki bez usuwania jej plików.
  - **Przycisk usuwania (Kosz)**: Bezpieczne odinstalowanie wtyczki z dysku z modalnym oknem potwierdzenia akcji.
  - **Kliknięcie pozycji**: Otwiera pełną stronę szczegółów rozszerzenia w nowej karcie edytora.

---

## Widok Szczegółów Rozszerzenia (Extension Details View)

Kliknięcie dowolnej wtyczki na liście otwiera dedykowaną kartę w stylu VS Code Extension Marketplace:

- **Natywna zakładka**: Karta oznaczona jest ikoną SVG wtyczki i tytułem `Extension: <Nazwa>`.
- **Baner główny (Hero Section)**:
  - Duża ikona rozszerzenia, nazwa, autor, wersja oraz plakietka typu (np. `Lua Plugin`).
  - Przycisk **Uninstall**: Usunięcie wtyczki z dysku.
  - Przycisk **Open Folder**: Natychmiastowe otwarcie folderu z plikami wtyczki w Eksploratorze Windows.
  - Przycisk **Edit Script**: Otwarcie pliku `init.lua` lub skryptu w edytorze kodu w celu natychmiastowej modyfikacji.
- **Zakładka DETAILS**: Wbudowany czytnik renderujący plik `README.md` wtyczki w czasie rzeczywistym z pełnym formatowaniem nagłówków, list punktowanych i bloków kodu.
- **Panel boczny (MORE INFO)**: Zestawienie metadanych (identyfikator, wersja, autor, ścieżka na dysku, plik dokumentacji).

---

## Izolacja środowisk

Każdy plugin otrzymuje **własny `lua_State`** (izolowane środowisko VM). Oznacza to:

- Błąd lub crash jednego skryptu nie wpływa na pozostałe.
- Pluginy nie widzą nawzajem swoich zmiennych globalnych.
- Każdy plugin ma własną kopię standardowej biblioteki Lua (`os`, `string`, `math`, `table` itp.).

---

## Rejestracja API

Po załadowaniu pliku, Luce automatycznie udostępnia globalną tabelę `luce` zawierającą pełne API edytora.
Skrypt może wtedy wywołać `luce.register_command(...)`, `luce.register_completion(...)` czy `luce.on(...)` bez żadnych importów.

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
      CommandPalette / TabBar / Editor / ...
```

---

## Cykl życia pluginu

| Etap             | Co się dzieje                                                    |
|------------------|------------------------------------------------------------------|
| **Init**         | `luaL_dofile()` — punkt wejścia (`init.lua` lub `.lua`) jest wykonywany od góry do dołu |
| **Rejestracja**  | Skrypt woła `luce.register_command(...)`, rejestruje hooki lub autouzupełnianie |
| **Tick**         | Luce woła globalną funkcję `on_tick(dt)` w każdej klatce (jeśli zdefiniowana) |
| **Shutdown**     | Luce woła `on_shutdown()` przy wyłączaniu, a następnie zamyka `lua_State` |

---

## Instalacja nowego pluginu

1. Utwórz folder `plugins/<twoja_wtyczka>/` z plikiem `init.lua` (i opcjonalnym `README.md`) lub pojedynczy plik `plugins/<twoja_wtyczka>.lua`.
2. Kliknij ikonę **`↻`** w panelu Plugins w sidebarze (lub polecenie **Theme / Plugins: Reload Custom Themes** w Command Palette).
3. Nowa wtyczka pojawi się natychmiast na liście i zostanie załadowana do pamięci bez restartowania Luce!

> Kliknij **"Open Plugins Folder"** w zakładce Plugins w sidebarze, aby szybko przejść do folderu wtyczek w Eksploratorze Windows.
