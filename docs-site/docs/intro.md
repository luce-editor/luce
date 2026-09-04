---
id: intro
title: Wprowadzenie i Szybki Start
sidebar_label: Wprowadzenie
slug: /intro
---

# Witamy w Edytorze Luce

**Luce** to niesamowicie szybki, lekki i nowoczesny edytor kodu napisany w **C++23** przy użyciu **Dear ImGui** (gałąź docking), **SDL2** oraz **OpenGL 3.3**.

Zaprojektowany z myślą o minimalizmie, bezkompromisowej responsywności i modularnej rozszerzalności, Luce łączy estetykę nowoczesnych edytorów z surową wydajnością i znikomym zużyciem pamięci RAM natywnej aplikacji pulpitu.

![Luce Logo](@site/static/img/luce-logo.png)

---

## Główne Funkcje

- ⚡ **Wydajność C++23:** Błyskawiczny czas startu poniżej sekundy, minimalne opóźnienia i znikome zużycie RAM.
- 🎨 **Bogaty Silnik Motywów:** Wbudowane palety VS Code Dark Modern, Catppuccin Mocha, One Dark i Nord, a także obsługa własnych plików CSS z funkcją **Hot Reload na żywo**.
- 🧩 **System Wtyczek Lua 5.4:** Skryptowe rozszerzenia edytora — wystarczy plik `.lua` w folderze `plugins/`, bez kompilacji i bez konfiguracji.
- 📐 **Elastyczny Menedżer Dokowania:** Oparty o Dear ImGui Docking, pozwalający dowolnie układać edytor, pasek boczny oraz dolne panele.
- 🖥️ **Wbudowany Terminal:** Wielosesyjny terminal VT100 z obsługą kart, PowerShell (`pwsh.exe`) / Bash, synchronizacją katalogu projektu i TrueColor.
- ⚠️ **Wykrywanie Problemów (Diagnostics):** Dedykowana zakładka Problems z nawigacją — podwójne kliknięcie przenosi bezpośrednio do wiersza z błędem.
- 🔍 **Command Palette i Quick Open:** Wyszukiwarka poleceń (`Ctrl+Shift+P`), szybkie otwieranie plików (`Ctrl+P`) oraz skok do linii (`:numer_linii`).
- 🔤 **Wirtualne Przewijanie:** Płynna edycja plików liczących ponad 100 000 linii w stałych 60+ FPS dzięki buforowi linii i inkrementalnym lekserom.

---

## Budowanie ze Źródeł

### Wymagania

- **CMake 3.20** lub nowszy
- **Kompilator C++23**:
  - Windows: MSVC v143 (Visual Studio 2022 17.4+) lub Clang 16+
  - Linux: GCC 13+ lub Clang 16+
  - macOS: Apple Clang 15+ lub LLVM Clang 16+
- Sterowniki z obsługą **OpenGL 3.3+**

Wszystkie zależności (SDL2, Dear ImGui Docking, nlohmann/json) pobierane są automatycznie przez CMake (`FetchContent`).

### Instrukcja Budowania (Windows / Visual Studio)

```powershell
# Sklonuj repozytorium
git clone https://github.com/luce-editor/luce.git
cd luce

# Wygeneruj projekt w trybie Release
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Zbuduj aplikację
cmake --build build --config Release

# Uruchom Luce
.\build\Release\luce.exe
```

### Instrukcja Budowania (Linux / macOS)

```bash
# Konfiguracja i kompilacja
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)

# Uruchomienie
./build/luce
```

---

## Domyślne Skróty Klawiszowe

| Skrót | Akcja | Opis |
| :--- | :--- | :--- |
| `Ctrl+Shift+P` | **Command Palette** | Otwiera paletę poleceń edytora i wtyczek |
| `Ctrl+P` | **Quick Open** | Szybkie wyszukiwanie i otwieranie plików projektu |
| `Ctrl+G` | **Przejdź do linii** | Skok do podanego numeru wiersza (`:linia`) |
| `Ctrl+O` | **Otwórz plik** | Systemowe okno wyboru pliku |
| `Ctrl+Shift+O` | **Otwórz folder** | Otwiera katalog projektu w Eksploratorze |
| `Ctrl+S` | **Zapisz plik** | Zapisuje aktywny bufor |
| `Ctrl+W` | **Zamknij kartę** | Zamyka aktualnie aktywną kartę |
| `Ctrl+Z` | **Cofnij (Undo)** | Cofa ostatnią zmianę w tekście |
| `Ctrl+Y` / `Ctrl+Shift+Z` | **Ponów (Redo)** | Ponawia cofniętą zmianę |
| `Ctrl+=` / `Ctrl++` | **Powiększ (Zoom In)**| Zwiększa rozmiar czcionki edytora |
| `Ctrl+-` | **Pomniejsz (Zoom Out)**| Zmniejsza rozmiar czcionki edytora |
| `Ctrl+0` | **Resetuj Zoom** | Przywraca domyślny rozmiar czcionki |
| `Ctrl+` ` | **Przełącz Terminal** | Pokazuje lub ukrywa dolny terminal |

---

## Licencja

Luce jest projektem otwartoźródłowym wydanym na licencji **GPL v3**.
