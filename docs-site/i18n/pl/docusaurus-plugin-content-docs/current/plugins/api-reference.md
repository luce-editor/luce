---
id: api-reference
title: Lua Plugin API Reference
sidebar_label: Referencja API
slug: /plugins/api-reference
---

# Lua Plugin API Reference

Luce udostępnia zaawansowaną architekturę wtyczek **Lua 5.4 (API 2.0)**, działającą w modelu zero-compilation. Wtyczki mogą nasłuchiwać zdarzeń cyklu życia edytora, manipulować buforami tekstu, zgłaszać własną diagnostykę/lintery, rejestrować dostawców autouzupełniania oraz integrować się z systemem operacyjnym.

Wszystkie funkcje edytora są dostępne w globalnej tabeli `luce`.

---

## Struktura Wtyczek i Pakiety Folderowe

Luce obsługuje dwa formaty dystrybucji wtyczek w katalogu `plugins/`:
1. **Pojedynczy skrypt**: `plugins/nazwa_wtyczki.lua`
2. **Pakiet katalogowy**: `plugins/nazwa_pakietu/init.lua` (z opcjonalnymi podmodułami i plikiem `manifest.json`)

Wtyczki można dynamicznie przeładowywać oraz włączać/wyłączać w czasie rzeczywistym w zakładce **Plugins** w bocznym pasku edytora.

---

## Event Hooki (`luce.on`)

Rejestracja funkcji zwrotnych reagujących na zdarzenia w edytorze:

```lua
luce.on(event_name, callback)
```

| Nazwa Zdarzenia | Argumenty Callbacku | Opis |
|---|---|---|
| `"file_opened"` | `(filepath)` | Wywoływane natychmiast po załadowaniu pliku do karty. |
| `"before_save"` | `(filepath)` | Wywoływane tuż przed zapisaniem bufora na dysk (np. formatery kodu). |
| `"after_save"` | `(filepath)` | Wywoływane po udanym zapisie pliku (np. lintery, kompilatory). |
| `"cursor_moved"`| `(line, column)` | Wywoływane przy zmianie pozycji głównego kursora (0-indexed). |
| `"text_changed"`| `(start_line, count)` | Wywoływane przy modyfikacji zawartości bufora tekstu. |

### Przykład: Automatyczne Formatowanie przy Zapisie
```lua
luce.on("before_save", function(path)
    -- Usunięcie zbędnych spacji z końców linii
    local text = luce.get_text()
    local cleaned = text:gsub("[ \t]+\n", "\n")
    if cleaned ~= text then
        luce.set_text(cleaned)
    end
end)
```

---

## Edycja Bufora i Dokumentu

### `luce.get_text() → string`
Zwraca całą treść aktywnego dokumentu.

### `luce.set_text(text)`
Podmienia całą zawartość bufora (z pełnym zachowaniem historii cofania Undo/Redo).

### `luce.get_line_count() → number`
Zwraca łączną liczbę linii w aktywnym dokumencie.

### `luce.get_line(n) → string`
Zwraca treść linii o numerze `n` (0-indexed).

### `luce.set_line(n, text)`
Ustawia treść linii `n` (0-indexed).

### `luce.replace_range(start_line, start_col, end_line, end_col, text)`
Precyzyjnie zamienia zadany fragment tekstu we współrzędnych bufora.

### `luce.insert_text(text)`
Wstawia tekst w bieżącym położeniu kursora.

### `luce.delete_selection()`
Usuwa aktualnie zaznaczony fragment tekstu.

### `luce.get_selection() → string`
Zwraca aktualnie zaznaczony tekst.

---

## Zarządzanie Plikami i Przestrzenią Roboczą

### `luce.open_file(path)`
Otwiera podaną bezwzględną ścieżkę pliku w nowej karcie edytora (lub przełącza na nią).

### `luce.save_file()`
Zapisuje bieżący dokument na dysku.

### `luce.close_tab()`
Zamyka aktywną kartę edytora.

### `luce.get_file_path() → string`
Zwraca pełną ścieżkę do aktywnego pliku (np. `"C:/Projekty/app/main.cpp"`).

### `luce.get_file_name() → string`
Zwraca nazwę pliku z rozszerzeniem (np. `"main.cpp"`).

### `luce.get_file_extension() → string`
Zwraca rozszerzenie pliku z kropką (np. `".cpp"`).

### `luce.get_workspace_path() → string`
Zwraca ścieżkę do otwartego folderu projektu.

### `luce.execute_command(command) → string`
Synchronicznie wykonuje polecenie systemowe w folderze projektu i zwraca jego wyjście:
```lua
local wynik = luce.execute_command("clang-format src/main.cpp")
```

---

## Rejestracja Komend i Skrótów Klawiszowych

### `luce.register_command(id, display_name, [shortcut], fn)`
Rejestruje polecenie w Command Palette (`Ctrl+Shift+P`).

| Parametr | Typ | Opis |
|---|---|---|
| `id` | `string` | Unikalny identyfikator polecenia (np. `"tools.format"`) |
| `display_name` | `string` | Tytuł widoczny w palecie komend |
| `shortcut` | `string` *(opcjonalny)* | Podpowiedź skrótu (np. `"Ctrl+Alt+F"`) |
| `fn` | `function` | Funkcja wykonywana po uruchomieniu komendy |

```lua
luce.register_command("tools.format", "Formatuj Dokument", "Ctrl+Alt+F", function()
    luce.show_notification("Formatowanie dokumentu...", "info")
end)
```

---

## Własna Diagnostyka (API dla Linterów)

Wtyczki mogą dodawać błędy, ostrzeżenia i podkreślenia (falbanki) bezpośrednio do panelu Problems:

### `luce.add_diagnostic(tabela)`
```lua
luce.add_diagnostic({
    file     = "C:/Projekty/src/main.cpp",
    line     = 42,            -- 1-indexed
    column   = 5,             -- 1-indexed
    message  = "Nieużywana zmienna 'x'",
    severity = "warning"      -- "error" | "warning" | "info"
})
```

### `luce.clear_diagnostics([file_path])`
Czyści diagnostykę dla wskazanego pliku (lub dla wszystkich plików, gdy brak argumentu).

---

## Dostawcy Autouzupełniania (Autocomplete)

Rejestracja inteligentnych podpowiedzi kodu zależnych od rozszerzenia pliku:

```lua
luce.register_completion_provider(".cpp", function(prefix, line, col)
    if prefix:sub(1, 1) == "s" then
        return { "std::string", "std::vector", "size_t", "static_cast<>()" }
    end
    return {}
end)
```

---

## Powiadomienia (Toasty) i Logowanie

- `luce.show_notification(message, [level], [duration])`: Wyświetla pływający dymek Toast (`"info"`, `"warn"`, `"error"`, `"success"`).
- `luce.show_info(msg)` / `luce.show_warning(msg)` / `luce.show_error(msg)`
- `luce.set_status(msg)`: Ustawia tekst na pasku stanu.
- `luce.log(msg)` / `luce.warn(msg)`: Zapisuje komunikaty do dziennika diagnostycznego.

---

## Callbacki Cyklu Życia

- `function on_tick(dt)`: Wywoływane co klatkę renderingu (`dt` w sekundach).
- `function on_shutdown()`: Wywoływane przed zamknięciem programu lub przeładowaniem wtyczki.
