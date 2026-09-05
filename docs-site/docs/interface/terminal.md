---
id: terminal
title: Wbudowany Terminal
sidebar_label: Wbudowany Terminal
slug: /interface/terminal
---

# Wbudowany Terminal

Luce posiada nowoczesny, w pełni interaktywny panel terminala zadokowany w dolnej części przestrzeni roboczej, oparty na natywnej pseudo-konsoli (ConPTY / PTY) oraz silniku `libvterm`.

---

## Architektura i Możliwości

Terminal w Luce nie jest prostym oknem tekstowym, lecz pełnoprawnym emulatorem terminala VT100:
- **Silnik emulacji**: Wykorzystuje bibliotekę `libvterm` do precyzyjnego parsowania sekwencji ANSI, kolorów TrueColor i stylów tekstu (pogrubienie, kursywa, podkreślenie).
- **Obsługa powłoki**:
  - **Windows**: Nowoczesny PowerShell (`pwsh.exe`) z automatycznym fallbackiem do `powershell.exe`.
  - **Linux / macOS**: Domyślna powłoka systemowa zdefiniowana w zmiennej `$SHELL` (np. `/bin/bash` lub `/bin/zsh`).
- **Synchronizacja katalogu roboczego (CWD)**: Terminal automatycznie uruchamia się w folderze aktualnie otwartym w Eksploratorze Plików Luce.

---

## Wielosesyjność (Karty Terminala)

Luce pozwala na pracę z wieloma równoległymi sesjami terminala:
- **Dodawanie nowej karty**: Przycisk `+` na pasku kart terminala natychmiast tworzy nową, niezależną powłokę.
- **Zamykanie sesji**: Każda karta posiada przycisk zamknięcia `×`, który natychmiast bezpiecznie zamyka dany podproces.
- **Numeracja sesji**: Gdy otwartych jest kilka terminali, karty są automatycznie numerowane (`1: pwsh`, `2: pwsh`, itd.), ułatwiając orientację w procesach (np. dev-server, kompilator, git).
- **Wyraziste wyróżnienie aktywnego terminala**: Aktywna karta terminala posiada jasny, czytelny tekst oraz niebieski pasek akcentujący (`#007ACC`) na górnej krawędzi, a nieaktywne karty pozostają stonowane.

---

## Czytelność i Kolorystyka

- **Wprowadzanie poleceń**: Wpisywane komendy i aliasy są renderowane w czystym, wyrazistym białym kolorze.
- **Podpowiedzi i predykcje (PSReadLine)**: Sugestie (np. inline prediction w PowerShell) są renderowane w czytelnym, stonowanym odcieniu szarości (`#787878`), idealnie widocznym na ciemnym tle.

---

## Zaznaczanie Tekstu i Schowek

Terminal w Luce obsługuje intuicyjne zaznaczanie tekstu za pomocą myszy (z podświetleniem zaznaczonego obszaru) oraz dedykowane skróty kopiowania i wklejania:
- **Zaznaczanie myszą**: Przytrzymaj lewy przycisk myszy i przeciągnij kursor nad tekstem w buforze terminala, aby go zaznaczyć.
- **Kopiowanie bez kolizji z SIGINT**: Aby skrót `Ctrl+C` mógł niezawodnie przerywać działające programy w konsoli, do kopiowania zaznaczenia służy dedykowany skrót `Win+Shift+C` (lub `Cmd+Shift+C` na macOS, `Ctrl+Shift+C` na Linuxie).
- **Wklejanie**: Szybkie wklejanie ze schowka systemowego za pomocą `Win+Shift+V` (lub `Cmd+Shift+V` / `Ctrl+Shift+V`) oraz standardowego `Ctrl+V`.

---

## Skróty Klawiszowe

| Skrót | Działanie |
| :--- | :--- |
| `Ctrl+` ` (Tylda / Backtick) | Przełącz widoczność dolnego panelu z terminalem |
| `Win+Shift+C` / `Ctrl+Shift+C` | Kopiowanie zaznaczonego tekstu do schowka |
| `Win+Shift+V` / `Ctrl+Shift+V` | Wklejanie tekstu ze schowka |
| `Ctrl+C` | Przerwanie aktywnego procesu powłoki (SIGINT) |
| `Tab` | Autouzupełnianie poleceń i ścieżek powłoki |
