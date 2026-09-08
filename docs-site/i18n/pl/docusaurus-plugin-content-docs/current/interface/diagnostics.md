---
id: diagnostics
title: Wykrywanie Błędów i Diagnostyka
sidebar_label: Diagnostyka i Problemy
slug: /interface/diagnostics
---

# Wykrywanie Błędów i Diagnostyka

Luce wyposażony jest w zintegrowany silnik diagnostyczny (**Diagnostic Engine**), który analizuje kod źródłowy w locie oraz przy zapisie za pomocą natywnych kompilatorów i linterów, prezentując wykryte błędy w czytelnej formie.

---

## Jak Działa Silnik Diagnostyczny

Silnik uruchamia bezobiektową analizę składniową (`syntax-only check`) w tle, nie spowalniając interfejsu użytkownika:

- **C / C++**: Uruchamia `clang++ -fsyntax-only -Wall` (z automatycznym fallbackiem do `g++` lub MSVC `cl.exe /Zs`).
- **Python**: Uruchamia kompilację bajtową składni `python -m py_compile`.
- **Rust**: Uruchamia sprawdzanie składni za pomocą `rustc --error-format=short`.

Wynik działania kompilatora jest przetwarzany przez uniwersalny parser Luce, który wyodrębnia plik, numer wiersza, kolumny oraz treść błędu lub ostrzeżenia.

---

## Prezentacja w Edytorze

### 1. Podkreślenia Faliste (Squiggly Underlines)
- Wiersze i tokeny zawierające błędy podświetlane są na czerwono charakterystyczną falistą linią (`~ ~ ~`).
- Ostrzeżenia i uwagi sygnalizowane są kolorem żółtym.
- **Dymek po najechaniu (Hover Tooltip)**: Po umieszczeniu kursora myszy nad podkreślonym kodem wyświetla się okienko z dokładną treścią komunikatu kompilatora.

### 2. Panel Problems (Dolny Dok)
- W dolnym panelu dokowalnym w zakładce **Problems** wyświetlana jest skumulowana lista wszystkich wykrytych problemów w projekcie.
- Każdy wpis zawiera ikonę wagi (błąd / ostrzeżenie), nazwę pliku, numer linii, kolumny oraz opis.
- **Nawigacja**: Dwukrotne kliknięcie na dowolny błąd natychmiast otwiera dany plik i ustawia kursor dokładnie w miejscu wystąpienia problemu.

---

## Uruchamianie Sprawdzania

1. **Automatycznie**: Przy każdym zapisie pliku (`Ctrl+S`).
2. **Skrótem klawiszowym**: **Ctrl+Shift+B** (weryfikuje bieżący plik).
3. **Z Command Palette**: Komenda `Diagnostics: Check Active File`.
