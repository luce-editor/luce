---
id: architecture
title: Główna Architektura
sidebar_label: Architektura
slug: /architecture
---

# Architektura Edytora Luce

Luce opiera się na ścisłej separacji warstw, efektywności pamięciowej zorientowanej na dane oraz renderowaniu interfejsu w trybie natychmiastowym (Immediate Mode).

```
┌────────────────────────────────────────────────────────────────┐
│                       Główna Pętla                             │
│                 (Pętla SDL2 + OpenGL 3.3)                      │
└───────────────────────────────┬────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│                     Powłoka Aplikacji (UI)                     │
│   ├── Poziomy Activity Bar (Eksplorator & Wtyczki)             │
│   ├── Menedżer Kart (TabBar) & Przestrzeń Dokowania            │
│   ├── Command Palette & Nakładka Wyszukiwania                  │
│   └── Panel Wbudowanego Terminala                              │
└───────────────────────────────┬────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌────────────────┐      ┌────────────────┐      ┌────────────────┐
│ Silnik Edytora │      │ Silnik Składni │      │ Plugin Manager │
│ ├── TextBuffer │      │ ├── Lexer C++  │      │ ├── Lua 5.4 VM  │
│ ├── Undo/Redo  │      │ ├── Lexer Rust │      │ ├── lua_State[] │
│ └── VirtualPos │      │ └── Line Cache │      │ └── luce.* API  │
└────────────────┘      └────────────────┘      └────────────────┘
```

---

## 1. Separacja Warstw

- **`src/editor/`**: Przechowywanie dokumentu i edycja tekstu. `TextBuffer` odpowiada za wiersze i stosy cofania/ponawiania. `EditorView` odpowiada za wirtualne przewijanie, zawijanie tekstu, zaznaczenia i kursor.
- **`src/syntax/`**: Stanowy silnik tokenizacji. Interfejs `Lexer` przetwarza tekst linijka po linijce, operując na stanach `LexerState`. `SyntaxHighlighter` zarządza pamięcią podręczną tokenów.
- **`src/ui/`**: Komponenty Dear ImGui, zarządzanie motywami (`ThemeManager`), eksplorator plików, karty oraz paleta poleceń.
- **`src/plugin/`**: Silnik skryptowy Lua 5.4. `PluginManager` skanuje folder `plugins/` i ładuje każdy plik `.lua` w osobnym, izolowanym `lua_State`. Tabela `luce.*` udostępnia pełne API edytora skryptom.
- **`src/platform.*`**: Warstwa abstrakcji systemu operacyjnego dla okien dialogowych i potoków procesów.

---

## 2. Wirtualne Przewijanie (Virtual Scrolling)

Przy plikach liczących 50 000+ linii iterowanie po każdym wierszu w każdej klatce powoduje drastyczne spadki płynności.

Luce rozwiązuje to poprzez **Virtual Scrolling**:
1. `EditorView` odczytuje pozycję suwaka `ImGui::GetScrollY()` oraz wysokość widocznego okna.
2. Wylicza zakres widocznych linii:
   ```
   StartLine = floor(ScrollY / LineHeight)
   EndLine   = StartLine + ceil(ViewportHeight / LineHeight) + 2
   ```
3. Tylko linie w tym przedziale `[StartLine, EndLine]` są mierzone, tokenizowane i renderowane.
4. Pozostałe setki tysięcy niewidocznych linii nie obciążają procesora ani karty graficznej.

---

## 3. Stanowa Inkrementalna Analiza Składni

Języki z konstrukcjami wieloliniowymi (np. komentarze blokowe `/* ... */`, surowe ciągi znaków `R"(...)"`) nie mogą być parsowane czysto bezstanowo wiersz po wierszu.

Interfejs `Lexer` w Luce wymaga implementacji:

```cpp
virtual LexerState TokenizeLine(
    const std::string& line, 
    LexerState initial_state, 
    std::vector<Token>& out_tokens
) = 0;
```

`SyntaxHighlighter` śledzi stan `LexerState` zwracany na końcu każdej linii:
- Jeśli linia `N` zostanie zmodyfikowana, tylko linie od `N` w dół są ponownie analizowane, dopóki stan `LexerState` nie zrówna się z wcześniej zapisanym stanem dla kolejnych linii.
- Wszystkie nienaruszone linie są natychmiast pobierane z pamięci podręcznej tokenów.
