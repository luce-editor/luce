---
sidebar_position: 1
title: Ekran powitalny (Welcome)
description: Przegląd minimalistycznego ekranu powitalnego Luce w stylu edytora Zed
---

# Ekran powitalny (Welcome Screen)

Luce wyposażony jest w natywny, minimalistyczny **ekran powitalny** (Welcome / Getting Started) wzorowany na nowoczesnych edytorach, takich jak **Zed**. Pojawia się on automatycznie przy pierwszym uruchomieniu programu lub gdy uruchomisz edytor bez otwartych plików, a także na żądanie z poziomu menu **Help > Welcome (Getting Started)** oraz Command Palette (`help.welcome`).

Interfejs ekranu powitalnego został zaprojektowany tak, aby idealnie współgrał ze stylem wizualnym Eksploratora plików, panelu Git oraz panelu Wtyczek – wykorzystuje jednolitą typografię, nagłówki sekcji wielkimi literami, subtelne separatory i wektorowe ikony SVG.

---

## Układ i sekcje

Ekran powitalny dzieli się na czytelne, zbalansowane obszary funkcjonalne:

### 1. Nagłówek (Header)
- **Tytuł**: Welcome to Luce
- **Podtytuł**: Fast, lightweight C++23 code editor with native UI

### 2. Rozpocznij pracę (Get Started)
Szybkie akcje pozwalające natychmiast rozpocząć pisanie kodu:
- **New File** (`Ctrl+N`): Tworzy nowy, pusty dokument.
- **Open Folder...** (`Ctrl+O`): Otwiera systemowe okno wyboru folderu i ustawia główny katalog projektu.
- **Clone Repository...** (`Git`): Otwiera wbudowane okno klonowania repozytorium Git bezpośrednio do nowego folderu roboczego.
- **Open Command Palette** (`Ctrl+Shift+P`): Otwiera paletę poleceń.

### 3. Ostatnie foldery (Recent Workspaces)
Lista ostatnio otwieranych katalogów projektów:
- Wyświetla nazwę projektu oraz skróconą ścieżkę do katalogu.
- Jedno kliknięcie natychmiast otwiera dany projekt, inicjalizuje repozytorium Git i skanuje pliki.

### 4. Konfiguracja (Configuration)
Dostęp do personalizacji programu:
- **Open Settings** (`Ctrl+,`): Otwiera niezależne natywne okno systemowe ustawień.
- **Keyboard Shortcuts** (`Keys`): Przenosi bezpośrednio do sekcji skrótów klawiszowych w ustawieniach.
- **Explore Extensions** (`Plugins`): Przełącza panel boczny na widok wtyczek Lua.
- **Color Themes** (`Theme`): Otwiera paletę poleceń z prefiksem `Theme:` umożliwiając natychmiastowy podgląd i zmianę motywu.

### 5. Pomoc i zasoby (Help & Resources)
Bezpośrednie odnośniki do dokumentacji i społeczności:
- **Documentation**: Otwiera oficjalną dokumentację Luce w przeglądarce.
- **GitHub Repository**: Przenosi do kodu źródłowego edytora na GitHubie.
- **Report an Issue**: Bezpośredni link do zgłaszania uwag i błędów.

### 6. Stopka i preferencje uruchamiania
Na dole strony znajduje się nowoczesny animowany przełącznik (toggle switch):
- **Show Welcome page on startup**: Gdy opcja jest włączona, ekran powitalny otwiera się automatycznie, gdy brak jest otwartych kart z plikami.

---

## Konfiguracja w settings.json

Zachowanie ekranu powitalnego oraz historię projektów można kontrolować także bezpośrednio w pliku `settings.json`:

```json
{
    "ui": {
        "show_welcome_on_startup": true
    },
    "recent_projects": [
        "c:/Users/Szymon/Desktop/luce",
        "c:/Projects/my-app"
    ]
}
```
