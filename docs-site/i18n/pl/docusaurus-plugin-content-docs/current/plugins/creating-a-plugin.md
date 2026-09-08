---
id: creating-a-plugin
title: Tworzenie Własnych Wtyczek (Lua)
sidebar_label: Poradnik Wtyczek
slug: /plugins/creating-a-plugin
---

# Tworzenie Własnych Wtyczek (Lua)

Luce pozwala na rozszerzanie edytora za pomocą prostego i szybkiego języka [Lua 5.4](https://www.lua.org/) — bez kompilacji, bez CMake i bez zewnętrznych zależności.

Pliki pluginów z rozszerzeniem `.lua` umieszczane są w katalogu `plugins/` obok pliku wykonywalnego `luce.exe`.

---

## Praktyczny przykład wtyczki (`plugins/skeletonCpp.lua`)

Poniższy skrypt rejestruje dwie komendy w Command Palette: wstawianie szkieletu funkcji `main()` w C++ oraz automatyczne generowanie nagłówka klasy na podstawie nazwy aktywnego pliku. Demonstruje również operacje na tekście oraz powiadomienia toast.

```lua
-- ============================================================================
--  skeletonCpp.lua -> wstawianie szkieletu C++ w miejscu kursora
--
--  Instalacja: wrzuć ten plik do katalogu plugins/
--  Użycie: Ctrl+Shift+P -> Skeleton: Insert Basic Skeleton
-- ============================================================================

-- 1. Metadane wtyczki (widoczne w panelu Plugins)
luce.plugin = {
    name = "Skeleton",
    version = "1.0.0",
    author = "Luce Team",
    description = "Wstawia podstawowy szkielet C++ oraz nagłówek klasy"
}

-- 2. Funkcja wstawiająca funkcję main()
local function insert_skel()
    local skelMesh = [[
#include <iostream>

int main() {
    
    return 0;
}
]]
    luce.insert_text(skelMesh)
    local file_name = luce.get_file_name()
    luce.show_notification(string.format("Skeleton inserted for %s", file_name), "success", 5)
end

-- 3. Funkcja generująca nagłówek klasy z dopasowaną nazwą pliku
local function insert_header_skel()
    local file_name = luce.get_file_name()
    local pos = string.find(file_name, "%.")
    local class_name = pos and string.sub(file_name, 1, pos - 1) or file_name
    local skelMesh = string.format([[
#pragma once

class %s {
public:
    %s();
    ~%s();
private:

};
]], class_name, class_name, class_name)

    luce.insert_text(skelMesh)
    luce.show_notification(string.format("Header inserted for %s", file_name), "success", 5)
end

-- 4. Rejestracja poleceń w Command Palette (Ctrl+Shift+P)
luce.register_command("insert_skeleton", "Skeleton: Insert Basic Skeleton", insert_skel)
luce.register_command("insert_header_skel", "Skeleton: Insert Header Skeleton", insert_header_skel)
```

---

## Cykl życia wtyczki

Każdy skrypt Lua jest ładowany w osobnym, izolowanym środowisku `lua_State`. Oprócz kodu wykonywanego od razu przy starcie, wtyczka może opcjonalnie zaimplementować funkcje zwrotne:

| Callback           | Kiedy jest wywoływany          | Zastosowanie |
|--------------------|--------------------------------|--------------|
| *Główny kod pliku* | Przy załadowaniu wtyczki       | Rejestracja komend i konfiguracja początkowa |
| `on_tick(dt)`      | W każdej klatce renderowania   | Okresowe sprawdzanie warunków, timery (`dt` = sekundy) |
| `on_shutdown()`    | Przed wyłączeniem edytora      | Zwalnianie zasobów lub zapisywanie stanu |

```lua
function on_tick(dt)
    -- Wywoływane co klatkę (np. do cyklicznych zadań w tle)
end

function on_shutdown()
    luce.log("Wtyczka zwalnia zasoby przed zamknięciem Luce.")
end
```

---

## Podręczna ściągawka API

Najważniejsze funkcje udostępniane przez globalny obiekt `luce`:

### Rejestracja komend
```lua
luce.register_command(id, display_name, function() ... end)
```

### Edycja tekstu
```lua
luce.insert_text(text)       -- Wstawia tekst w miejscu kursora
luce.delete_selection()      -- Usuwa aktualne zaznaczenie
luce.get_selection()         -- Zwraca zaznaczony tekst (string)
luce.get_line(n)             -- Zwraca tekst wiersza n (indeksowany od 0)
```

### Kursor i plik
```lua
luce.get_cursor_line()       -- Numer aktualnej linii (indeksowany od 0)
luce.get_cursor_column()     -- Numer kolumny (indeksowany od 0)
luce.set_cursor(line, col)   -- Ustawia kursor na podane współrzędne
luce.get_file_path()         -- Pełna ścieżka do aktywnego pliku
luce.get_file_name()         -- Nazwa pliku z rozszerzeniem (np. main.cpp)
luce.get_file_extension()    -- Rozszerzenie pliku bez kropki (np. cpp)
```

### Powiadomienia, status i logowanie
```lua
luce.show_notification(msg, type, duration) -- Toast ("info", "success", "warning", "error")
luce.set_status(text)                       -- Wyświetla komunikat w pasku stanu
luce.log(text)                              -- Log do konsoli [Lua Plugin INFO]
luce.warn(text)                             -- Ostrzeżenie [Lua Plugin WARN]
```

> Pełny opis wszystkich typów i sygnatur parametrów znajdziesz w dokumencie **[Referencja API Lua](./api-reference)**.

---

## Jak dodać nową wtyczkę do Luce?

Aby dodać nową wtyczkę bez konieczności restartowania programu:

1. Otwórz edytor Luce i przejdź do zakładki **Plugins** (ikona puzzla na pasku aktywności).
2. Kliknij przycisk **Open Plugins Folder** u dołu panelu — otworzy się katalog `plugins/` w menedżerze plików systemu.
3. Skopiuj lub zapisz swój plik `.lua` w tym folderze.
4. Kliknij przycisk **Reload Plugins** w edytorze.
5. Twoja wtyczka natychmiast pojawi się na liście zainstalowanych wtyczek, a jej komendy będą dostępne w **Command Palette** (`Ctrl+Shift+P`).
