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
2. **Pasek Boczny (Sidebar)**: Zintegrowany z poziomym Activity Barem (drzewo katalogów oraz wtyczki).
3. **Menedżer Kart & Edytor**: Płynne otwieranie wielu plików, dzielenie widoku w pionie/poziomie, podgląd Markdown na żywo oraz podgląd obrazów.
4. **Dolny Panel Wielozadaniowy**:
   - **Problems (Problemy)**: Centralna lista wykrytych błędów i ostrzeżeń w projekcie. Podwójne kliknięcie dowolnego problemu natychmiast otwiera odpowiedni plik i przenosi kursor bezpośrednio do wskazanej linii.
   - **Output (Wyjście)**: Dedykowana konsola wyjściowa procesów i narzędzi.
   - **Terminal**: Wbudowane taby terminala z emulacją VT100 i wsparciem dla PowerShell/Bash.
5. **Pasek Stanu**: Informacje o języku, pozycji kursora, kodowaniu, wcięciach i wersji edytora.

---

## Zapamiętywanie Układu (Docking) i Stanu Sesji

- **Układ okien**: Pozycje i rozmiary zadokowanych paneli są automatycznie zapisywane do pliku `imgui.ini`.
- **Sesja projektu**: Otwarty katalog główny, lista aktywnych kart plików oraz skala interfejsu są automatycznie zapamiętywane w `session.json` i przywracane przy ponownym uruchomieniu edytora.
