---
id: command-palette
title: Command Palette i Szybkie Otwieranie
sidebar_label: Command Palette
slug: /interface/command-palette
---

# Command Palette i Szybkie Otwieranie

**Command Palette** stanowi centralne centrum sterowania edytorem Luce, umożliwiające błyskawiczny dostęp do poleceń, plików oraz nawigacji bez odrywania rąk od klawiatury.

---

## Tryby i Skróty

| Skrót | Tryb | Zastosowanie |
| :--- | :--- | :--- |
| `Ctrl+Shift+P` | **Polecenia** | Wyszukiwanie i wykonywanie dowolnych poleceń edytora, motywów oraz wtyczek Lua |
| `Ctrl+P` | **Quick Open** | Błyskawiczne wyszukiwanie rozmyte (fuzzy search) i otwieranie plików z projektu |
| `Ctrl+G` | **Przejdź do linii** | Skok bezpośrednio do wybranego numeru wiersza (np. `:42`) |

---

## Nawigacja i Wygoda Użytkowania

- **Płynne przewijanie**: Nawigacja strzałkami (`Góra` / `Dół`) automatycznie przewija listę wyników i utrzymuje aktualnie zaznaczony element w centrum widoku.
- **Wyszukiwanie rozmyte (Fuzzy Matching)**: Wpisywanie fragmentów słów (np. `ter` dla `Toggle Terminal` lub `app` dla `src/ui/app.cpp`) natychmiast filtruje dopasowane wyniki.
- **Bezramkowy, nowoczesny wygląd**: Nakładka palety została zintegrowana ze schematem kolorystycznym edytora bez zbędnych, jaskrawych obrysów.

---

## Rejestracja Własnych Komend (C++)

W metodzie `App::RegisterCommands()` (`src/ui/app.cpp`) można łatwo dodać nowe polecenia:

```cpp
command_palette_.RegisterCommand({
    "file.save_all",
    "File: Save All",
    "Ctrl+K S", // Opcjonalny skrót klawiszowy
    [this]() {
        // Logika akcji
    }
});
```
