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
| `Ctrl+Shift+P` | **Polecenia** | Z prefiksem `>`. Wyszukiwanie i wykonywanie dowolnych poleceń edytora, motywów oraz wtyczek Lua |
| `Ctrl+P` | **Quick Open** | Błyskawiczne wyszukiwanie rozmyte (fuzzy search) i otwieranie plików z projektu |
| `Ctrl+Shift+F` | **Wyszukiwanie w projekcie** | Find in Files w całym projekcie z podglądem linii i bezpośrednim skokiem |
| `Ctrl+G` | **Przejdź do linii** | Skok bezpośrednio do wybranego numeru wiersza (prefiksem `:`) |

---

## Prompt `>` i Dynamiczne Przełączanie Trybów

Pole tekstowe Command Palette dynamicznie reaguje na wpisywane znaki:
- **Prefiks `>` dla poleceń**: Otwarcie przez `Ctrl+Shift+P` automatycznie wpisuje znak `>`. Skasowanie `>` przełącza paletę na wyszukiwanie plików (*Quick Open*). Wpisanie `>` na początku ponownie włącza tryb poleceń.
- **Prefiks `:` dla skoku do linii**: Wpisanie dwukropka aktywuje tryb *Go to Line*.
- **Prefiks `?` lub `Ctrl+Shift+F` dla wyszukiwania w projekcie**: Przeszukiwanie tekstu we wszystkich plikach roboczych.

---

## Wyszukiwanie w Całym Projekcie (`Ctrl+Shift+F`)

Wciśnięcie `Ctrl+Shift+F` uruchamia **Find in Files**:
- **Głębokie indeksowanie projektu**: Przeszukuje zawartość plików roboczych z automatycznym pominięciem plików binarnych, `.git`, `build` oraz `node_modules`.
- **Dwu-wierszowy układ wyników**: Wyświetla relatywną ścieżkę i numer linii na górze, a poniżej czytelny wycinek kodu ze znalezionym tekstem.
- **Natychmiastowy skok do kodu**: Wybór trafienia klawiszem `Enter` lub myszą natychmiast otwiera plik i precyzyjnie pozycjonuje kursor na wskazanej linii i kolumnie.

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
