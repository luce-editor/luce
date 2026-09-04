---
id: creating-a-plugin
title: Tworzenie Wtyczki w Lua
sidebar_label: Poradnik Wtyczek
slug: /plugins/creating-a-plugin
---

# Tworzenie Wtyczki w Lua

Luce obsługuje skrypty Lua 5.4 jako wtyczki — bez kompilacji, bez konfiguracji CMake.
Wystarczy napisać plik `.lua` i wrzucić go do folderu `plugins/` obok `luce.exe`.

---

## Minimalny plugin

```lua
-- plugins/hello.lua

-- Metadane wtyczki (opcjonalne, ale zalecane)
luce.plugin = {
    name        = "Hello Plugin",
    version     = "1.0.0",
    author      = "Twój nick",
    description = "Wyświetla powitanie w statusbarze.",
}

-- Rejestracja komendy widocznej w Command Palette (Ctrl+Shift+P)
luce.register_command("hello", "Hello: Powitaj świat", function()
    luce.set_status("Witaj świecie!")
    luce.log("Hello plugin executed!")
end)
```

---

## Cykl życia wtyczki

| Callback           | Kiedy wywoływany               |
|--------------------|-------------------------------|
| Plik wykonywany natychmiast | Przy ładowaniu (start edytora) |
| `on_tick(dt)`      | Każdą klatkę (opcjonalny)     |
| `on_shutdown()`    | Przy wyłączeniu edytora (opcjonalny) |

```lua
-- Opcjonalne callbacki cyklu życia

function on_tick(dt)
    -- dt = czas od ostatniej klatki w sekundach
end

function on_shutdown()
    luce.log("Moja wtyczka się wyłącza.")
end
```

---

## Pełne API — tabela `luce`

### Komendy

```lua
luce.register_command(id, display_name, function() ... end)
```
Rejestruje komendę w Command Palette (`Ctrl+Shift+P`).

---

### Edycja tekstu

```lua
luce.insert_text(text)       -- wstawia tekst w miejscu kursora
luce.delete_selection()      -- usuwa zaznaczony tekst
luce.get_selection()         -- zwraca zaznaczony tekst (string)
luce.get_line(n)             -- zwraca tekst linii o numerze n (0-indexed)
```

### Kursor i plik

```lua
luce.get_cursor_line()       -- numer aktualnej linii (0-indexed)
luce.get_cursor_column()     -- numer kolumny (0-indexed)
luce.set_cursor(line, col)   -- przesuwa kursor
luce.get_file_path()         -- ścieżka do aktualnie otwartego pliku
luce.get_file_name()         -- zwraca nazwe pliku (index.html, main.cpp itd)
luce.get_file_extension()    -- zwraca rozszerzenie pliku (hpp, html)
```

### Status i logowanie

```lua
luce.set_status(text)        -- wyświetla tekst w statusbarze
luce.log(text)               -- loguje info do konsoli [Lua Plugin INFO]
luce.warn(text)              -- loguje ostrzeżenie [Lua Plugin WARN]
```

---

## Przykład: zamiana zaznaczenia na UPPERCASE

```lua
-- plugins/uppercase.lua
luce.plugin = {
    name        = "UPPERCASE Converter",
    version     = "1.0.0",
    author      = "Luce Team",
    description = "Zamienia zaznaczony tekst na UPPERCASE.",
}

luce.register_command("uppercase", "UPPERCASE: Zamień zaznaczenie", function()
    local text = luce.get_selection()
    if text == "" then
        luce.set_status("Brak zaznaczenia!")
        return
    end
    luce.delete_selection()
    luce.insert_text(text:upper())
end)
```

---

## Jak zainstalować plugin

1. Skopiuj plik `.lua` do folderu `plugins/` obok `luce.exe`.
2. Uruchom (lub zrestartuj) edytor.
3. Otwórz `Ctrl+Shift+P` i wpisz nazwę komendy zarejestrowanej w pluginie.

> **Wskazówka:** Możesz też kliknąć przycisk **"Open Plugins Folder"** w zakładce Plugins w sidebarze.
