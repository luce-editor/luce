---
id: settings
title: Ustawienia i Preferencje
sidebar_label: Ustawienia
slug: /interface/settings
---

# Ustawienia i Preferencje

Luce oferuje przejrzysty system konfiguracji oparty na czytelnym formacie JSON, połączony z niezależnym natywnym oknem ustawień inspirowanym edytorami **Zed** oraz **VS Code**.

---

## Otwieranie Ustawień

Dostęp do konfiguracji można uzyskać na dwa wygodne sposoby:

1. **Natywne okno ustawień (UI)**:
   - Skrót klawiszowy: `Ctrl + ,`
   - Menu główne: **File -> Settings**
   - Paleta komend: `Preferences: Open Settings (UI)`
   - Ekran powitalny: przycisk **Open Settings**
   - Otwiera się w **osobnym, natywnym oknie systemowym Windows** z ciemnym paskiem tytułu, systemowymi przyciskami minimalizacji, maksymalizacji i zamykania oraz pełną obsługą wielu monitorów.
2. **Bezpośredni plik konfiguracyjny JSON**:
   - Skrót klawiszowy: `Ctrl + Shift + ,`
   - Menu główne: **File -> Open Settings File**
   - Alternatywnie kliknij przycisk **`{ } Open settings.json`** w prawym górnym rogu okna ustawień.

---

## Natywne Okno i Nowoczesne Kontrolki

### Niezależne okno OS (styl Zed)
Okno ustawień działa w osobnym oknie systemowym:
- **Wygodna wielozadaniowość**: Przenieś okno ustawień na drugi monitor lub przypnij je obok edytora Luce (Snap Assist).
- **Ciemny pasek tytułu Windows**: Zintegrowany z trybem ciemnym Windows 10/11 (`DWMWA_USE_IMMERSIVE_DARK_MODE`).
- **Czyste zakładki edytora**: Zakładki w Luce służą wyłącznie do pracy z plikami kodu źródłowego.
- **Dwukierunkowa synchronizacja na żywo**: Każda zmiana w oknie natychmiast wpływa na edytor, a ręczna edycja `settings.json` przeładowuje się w czasie rzeczywistym.

### Nowoczesne przełączniki (Toggle Switches)
Tradycyjne checkboxy zostały zastąpione nowoczesnymi suwakami w kształcie kapsułki z płynną mikro-animacją (`ImLerp`) i kursorem w kształcie łapki:
- **Lewa kolumna**: Tytuł opcji oraz zawijany opis funkcjonalności.
- **Prawa kolumna**: Przełącznik wyrównany bezpośrednio do prawej krawędzi karty.

---

## Przechowywanie i lokalizacja pliku

Wszystkie preferencje użytkownika zapisywane są w pliku `settings.json`, znajdującym się w katalogu programu Luce (lub w profilu aplikacji na innych platformach).

Wszelkie zmiany wprowadzone w graficznym interfejsie Settings są natychmiast zapisywane do `settings.json`. Z kolei bezpośrednia edycja i zapisanie pliku `settings.json` powoduje natychmiastowe przeładowanie i zaaplikowanie parametrów w działającym edytorze w czasie rzeczywistym.

### Przykładowy schemat JSON

```json
{
  "editor": {
    "font_size": 16,
    "tab_size": 4,
    "use_spaces": true,
    "show_minimap": false,
    "show_line_numbers": true,
    "highlight_current_line": true,
    "zoom_with_mouse_wheel": true,
    "cursor_blinking": true
  },
  "ui": {
    "font_size": 15,
    "scale": 1.0,
    "theme": "VS Code Dark 2026"
  },
  "general": {
    "auto_save": "off",
    "show_welcome_on_startup": true
  }
}
```

---

## Kategorie Ustawień

Interfejs ustawień organizuje preferencje w przejrzyste sekcje:

### 1. Ustawienia Edytora Tekstu
- **Editor: Font Size**: Płynny suwak rozmiaru czcionki kodu (10 do 36 px), przyciski krokowe `-` / `+` oraz przycisk `Reset (15px)` z podglądem składni na żywo.
- **Editor: Mouse Wheel Zoom**: Przełącznik płynnego skalowania czcionki rolką myszy z wciśniętym klawiszem `Ctrl`.
- **Editor: Tab Size**: Szybki wybór szerokości wcięcia (2, 4 lub 8 spacji).
- **Editor: Insert Spaces**: Przełącznik wstawiania spacji zamiast twardych znaków tabulacji (`\t`).
- **Editor: Minimap**: Przełącznik podglądu kodu w postaci minimapy (`Ctrl+M`).
- **Editor: Line Numbers**: Przełącznik wyświetlania numerów linii na marginesie.
- **Editor: Highlight Active Line**: Przełącznik podświetlania tła bieżącej linii.
- **Editor: Cursor Blinking**: Przełącznik animacji mrugania kursora tekstowego.

### 2. Wygląd i Motywy
- **UI: Text Font Size**: Niezależna zmiana rozmiaru czcionki interfejsu (menu, boczny pasek, zakładki, pasek stanu) z przyciskami `-`, `+` i `Reset (15px)`.
- **Window: Interface Zoom / Scale**: Globalna skala interfejsu (75% do 200%) z szybkimi presetami 100%, 125%, 150% oraz `Reset (100%)`.
- **Workbench: Color Theme Gallery**: Wizualne kafelki motywów z paletami 6 kolorów. Kliknięcie natychmiast aktywuje motyw.
- **Workbench: File & Folder Icons**: Zarządzanie własnymi ikonami SVG/PNG, otwieranie katalogu `icons/` oraz pliku reguł `icons.json` z przeładowywaniem na gorąco.

### 3. Ogólne
- **Files: Auto Save**: Konfiguracja automatycznego zapisu:
  - `Off (manual save)`
  - `On Window Focus Lost`
  - `After 1s Delay`
- **Startup: Show Welcome Page**: Przełącznik automatycznego wyświetlania ekranu powitalnego przy starcie, gdy żaden plik nie jest otwarty.
- **Configuration: Storage File**: Ścieżka do pliku `settings.json` z przyciskami **Open in Editor** oraz **Reveal in File Explorer**.

### 4. Skróty Klawiszowe
Wyszukiwalna i filtrowalna tabela wszystkich wbudowanych skrótów klawiszowych posortowanych według kategorii.

### 5. Wtyczki i Rozszerzenia
Przegląd zainstalowanych wtyczek Lua, szybki dostęp do folderu `plugins/` oraz zarządzanie rozszerzeniami.

### 6. O programie Luce
Diagnostyka zużycia pamięci RAM na żywo (Working Set) oraz zestawienie technologii silnika C++23.
