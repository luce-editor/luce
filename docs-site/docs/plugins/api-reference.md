---
id: api-reference
title: Lua Plugin API Reference
sidebar_label: Referencja API
slug: /plugins/api-reference
---

# Lua Plugin API Reference

Wszystkie funkcje dostępne dla skryptów Lua są umieszczone w globalnej tabeli `luce`.

---

## Metadane wtyczki

Opcjonalna tabela definiująca metadane widoczne w panelu Plugins:

```lua
luce.plugin = {
    name        = "Nazwa Wtyczki",   -- wyświetlana w panelu Plugins
    version     = "1.0.0",           -- wersja (dowolny string)
    author      = "Twój nick",       -- autor
    description = "Co ta wtyczka robi.", -- krótki opis
}
```

---

## Komendy

### `luce.register_command(id, display_name, fn)`

Rejestruje komendę widoczną w Command Palette (`Ctrl+Shift+P`).

| Parametr       | Typ      | Opis                                          |
|----------------|----------|-----------------------------------------------|
| `id`           | `string` | Unikalny identyfikator (np. `"my_cmd"`)       |
| `display_name` | `string` | Nazwa widoczna w palecie (np. `"My: Action"`) |
| `fn`           | `function` | Funkcja wywoływana po wybraniu komendy      |

```lua
luce.register_command("say_hi", "Moja Wtyczka: Przywitaj się", function()
    luce.insert_text("Cześć!")
end)
```

---

## Edycja tekstu

### `luce.insert_text(text)`
Wstawia podany tekst w miejscu kursora.

### `luce.delete_selection()`
Usuwa aktualnie zaznaczony tekst.

### `luce.get_selection() → string`
Zwraca aktualnie zaznaczony tekst. Jeśli nic nie jest zaznaczone, zwraca `""`.

### `luce.get_line(n) → string`
Zwraca tekst linii o podanym numerze (indeksowanie od `0`).

---

## Kursor

### `luce.get_cursor_line() → number`
Zwraca numer aktualnej linii kursora (indeksowanie od `0`).

### `luce.get_cursor_column() → number`
Zwraca numer aktualnej kolumny kursora (indeksowanie od `0`).

### `luce.set_cursor(line, col)`
Przesuwa kursor na podaną pozycję.

---

## Plik

### `luce.get_file_path() → string`
Zwraca pełną ścieżkę do aktualnie otwartego pliku (np. `"C:/Projects/app/src/main.cpp"`). Jeśli żaden plik nie jest otwarty, zwraca `""`.

### `luce.get_file_name() → string`
Zwraca samą nazwę aktualnego pliku wraz z rozszerzeniem (np. `"main.cpp"` lub `"Untitled-1"`). Jeśli edytor jest pusty, zwraca `""`.

### `luce.get_file_extension() → string`
Zwraca samo rozszerzenie aktualnego pliku wraz z kropką (np. `".cpp"` lub `".lua"`). Jeśli plik nie ma rozszerzenia lub nie jest zapisany, zwraca `""`.

---

## Status i Powiadomienia (Toasty)

### `luce.set_status(text)`
Wyświetla powiadomienie Toast w lewym dolnym rogu edytora (styl VS Code) oraz drukuje komunikat w konsoli.

### `luce.show_notification(message, [level], [duration])`
Wyświetla wyskakujące okienko Toast w lewym dolnym rogu ekranu z automatycznym zanikaniem.
- `message` (`string`): treść powiadomienia.
- `level` (`string`, opcjonalny): `"info"`, `"warn"`, `"error"` lub `"success"` (domyślnie `"info"`).
- `duration` (`number`, opcjonalny): czas wyświetlania w sekundach (domyślnie `4.0s`).

### `luce.show_error(text)`
Wyświetla czerwone powiadomienie Toast o błędzie.

### `luce.show_warning(text)`
Wyświetla żółte powiadomienie Toast z ostrzeżeniem.

### `luce.show_info(text)`
Wyświetla niebieskie powiadomienie informacyjne Toast.

### `luce.log(text)`
Wypisuje wiadomość info do konsoli (`[Lua Plugin INFO] ...`).

### `luce.warn(text)`
Wypisuje ostrzeżenie do konsoli (`[Lua Plugin WARN] ...`).

---

## Callbacki cyklu życia

Opcjonalne funkcje globalne, które Luce wywoła automatycznie:

### `function on_tick(dt)`
Wywoływana każdą klatkę. Parametr `dt` to czas od ostatniej klatki w sekundach.

```lua
function on_tick(dt)
    -- unikaj ciężkiej pracy tutaj — to jest hot path!
end
```

### `function on_shutdown()`
Wywoływana przy zamykaniu edytora (lub przy ręcznym odładowaniu wtyczki).

```lua
function on_shutdown()
    luce.log("Wtyczka zamknięta.")
end
```
