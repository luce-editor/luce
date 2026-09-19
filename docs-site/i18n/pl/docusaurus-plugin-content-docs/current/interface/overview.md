---
id: overview
title: Interfejs i Układ Okien
sidebar_label: Przegląd
slug: /interface/overview
---

# Zarządzanie Interfejsem i Układ Okien

Interfejs Luce bazuje na silniku **Dear ImGui (gałąź docking)** wzbogaconym o minimalistyczny styl nowoczesnych edytorów kodu.

---

## Główne Obszary Robocze

1. **Pasek Menu**: Zarządzanie plikami, edycją, widokiem, motywami oraz narzędziami.
2. **Pasek Boczny (Sidebar)**: Zintegrowany z poziomym Activity Barem (drzewo plików, Source Control Git oraz wtyczki Lua).
3. **Menedżer Kart & Edytor**: Płynne otwieranie wielu plików, tryb Split View (`Ctrl+\`), podgląd Markdown na żywo (`Ctrl+Shift+M`), minimapa (`Ctrl+M`), wskaźniki Git Gutter na marginesie oraz podgląd obrazów.
4. **Command Palette & Wyszukiwanie**: Szybka konsola komend z promptem `>` (`Ctrl+Shift+P`), Quick Open (`Ctrl+P`) oraz Wyszukiwanie w całym projekcie (`Ctrl+Shift+F`).
5. **Dolny Panel Wielozadaniowy**:
   - **Problems (Problemy)**: Centralna lista wykrytych błędów i ostrzeżeń w projekcie z opcją kopiowania.
   - **Output (Wyjście)**: Dedykowana konsola wyjściowa procesów i narzędzi.
   - **Terminal**: Wbudowane taby terminala z emulacją VT100 i wsparciem dla PowerShell/Bash.
6. **Pasek Stanu**: Dynamicznie stylizowany przez aktywny motyw pasek z informacjami o gałęzi Git, języku, pozycji kursora (Ln, Col), kodowaniu, wcięciach i wersji edytora.
7. **Ustawienia i Preferencje**: Niezależne natywne okno systemowe ustawień (`Ctrl+,`) w stylu edytora Zed z animowanymi przełącznikami toggle switch, galerią motywów oraz bezpośrednią edycją `settings.json` (`Ctrl+Shift+,`) z przeładowywaniem w czasie rzeczywistym.

---

## Split View (Dzielenie Edytora Obok Siebie)

Wciśnięcie skrótu `Ctrl+\` lub wybranie **View: Toggle Split Editor** dzieli obszar edytora na dwa niezależne, zsynchronizowane panele:
- Jednoczesna edycja dwóch różnych plików (np. `main.cpp` po lewej stronie i `test.hpp` po prawej stronie).
- Podgląd kodu obok sformatowanego dokumentu Markdown.
- Niezależne przewijanie, zaznaczanie, kursory i autouzupełnianie w każdym panelu.
- Szybki wybór wyświetlanego pliku przez rozwijaną listę w nagłówku prawego panelu.

---

## System Drag & Drop

Luce oferuje zaawansowany, płynny system przeciągania i upuszczania (Drag & Drop):
- **Zmienianie kolejności kart (Tab Reordering)**: Chwyć dowolną kartę na pasku i przeciągnij ją poziomo, aby zmienić kolejność plików. Upuszczenie na przycisk `+` dodaje kartę na końcu.
- **Przeciąganie karty do Split View**: Przeciągnięcie karty w prawy obszar edytora (strefa 35% szerokości z prawej strony) automatycznie dzieli edytor na dwa panele obok siebie i otwiera ten plik po prawej stronie.
- **Przenoszenie między panelami**: W trybie Split View przeciągaj karty lub pliki bezpośrednio do lewego lub prawego panelu.
- **Przeciąganie z Drzewa Plików (File Explorer)**: Przeciągaj pliki z drzewa projektu na pasek kart (otwarcie w wybranym miejscu), do obszaru edytora (otwarcie lub podział obok) lub do panelu terminala (wklejenie pełnej ścieżki).
- **Nowoczesne podświetlenia (Drop Overlays)**: Półprzezroczyste niebieskie nakładki podświetlające z ramką akcentową i etykietami (`Split Right`, `Left Pane`, `Right Pane`) precyzyjnie wskazują miejsce docelowe upuszczenia.

---

## Zapamiętywanie Układu (Docking) i Stanu Sesji

- **Układ okien**: Pozycje i rozmiary zadokowanych paneli są automatycznie zapisywane do pliku `imgui.ini`.
- **Sesja projektu**: Otwarty katalog główny, lista aktywnych kart plików oraz skala interfejsu są automatycznie zapamiętywane w `session.json` i przywracane przy ponownym uruchomieniu edytora.
