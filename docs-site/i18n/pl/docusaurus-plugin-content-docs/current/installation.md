---
title: Instalacja i konfiguracja
description: Jak zainstalować Luce za pomocą instalatora Windows, wersji portable lub skompilować ze źródeł.
sidebar_position: 2
---

# Instalacja i konfiguracja

Edytor Luce dostępny jest jako zautomatyzowany instalator Windows (`.exe`), samodzielna paczka przenośna (`.zip`) lub może zostać skompilowany bezpośrednio ze źródeł.

---

## Windows

### Metoda 1: Instalator Windows (Zalecana)

1. Przejdź do zakładki [Luce Releases](https://github.com/luce-editor/luce/releases) na GitHubie.
2. Pobierz najnowszą wersję instalatora: `Luce-Setup-x64.exe`.
3. Uruchom instalator:
   - **Instalacja dla użytkownika (Domyślna)**: Instaluje do `%LOCALAPPDATA%\Programs\Luce` bez konieczności posiadania praw administratora.
   - **Integracja z menu kontekstowym**: Automatycznie dodaje opcję *„Otwórz w Luce”* dla plików i katalogów w Eksploratorze Windows.
   - **Wiersz poleceń (`PATH`)**: Opcjonalnie dodaje Luce do zmiennej środowiskowej `PATH`, co pozwala uruchamiać edytor z terminala poleceniem `luce .` lub `luce <plik>`.
   - **Skróty**: Tworzy skróty w Menu Start oraz na Pulpicie z oficjalną ikoną programu.

### Metoda 2: Wersja przenośna (Portable)

Jeśli nie chcesz instalować programu w systemie:
1. Pobierz archiwum `Luce-win64-portable.zip` z sekcji [Releases](https://github.com/luce-editor/luce/releases).
2. Rozpakuj archiwum do dowolnego katalogu.
3. Uruchom `luce.exe`. Wszystkie ustawienia i stan sesji będą przechowywane lokalnie obok pliku wykonywalnego.

---

## Kompilacja ze źródeł

### Wymagania wstępne

- **CMake**: Wersja 3.20 lub nowsza
- **Kompilator C++23**:
  - Windows: Visual Studio 2022 (MSVC v17.10+) z pakietem *Desktop development with C++*
  - Linux: GCC 13+ lub Clang 16+
- **OpenGL**: Karta graficzna wspierająca OpenGL 3.3 lub nowszy
- **Git**: Niezbędny do automatycznego pobierania zależności przez `FetchContent`

### Kompilacja na Windows (MSVC)

```powershell
# Sklonuj repozytorium
git clone https://github.com/luce-editor/luce.git
cd luce

# Konfiguracja projektu
cmake -B build

# Kompilacja wersji Release
cmake --build build --config Release

# Uruchomienie
.\build\Release\luce.exe
```

### Kompilacja na Linux (Ubuntu / Debian)

```bash
# Instalacja pakietów deweloperskich
sudo apt update
sudo apt install -y build-essential cmake git \
    libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Klonowanie i konfiguracja
git clone https://github.com/luce-editor/luce.git
cd luce
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Kompilacja
cmake --build build -j$(nproc)

# Uruchomienie
./build/luce
```

Wszystkie biblioteki zewnętrzne (Dear ImGui gałąź docking, SDL2, Lua 5.4, libvterm) oraz pliki krojów pism są pobierane i linkowane automatycznie podczas konfiguracji CMake.
