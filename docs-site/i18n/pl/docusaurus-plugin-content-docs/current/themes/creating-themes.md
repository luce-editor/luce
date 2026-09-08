---
id: creating-themes
title: Tworzenie Własnych Motywów
sidebar_label: Własne Motywy (CSS)
slug: /themes/creating-themes
---

# Tworzenie Własnych Motywów (CSS)

Luce pozwala na tworzenie własnych motywów przy użyciu intuicyjnego formatu przypominającego CSS.

Pliki motywów z rozszerzeniem `.css` lub `.lucetheme` umieszczane są w katalogu `themes/`.

---

## Przykładowy Plik Motywu (`themes/cyberpunk.css`)

```css
/* Motyw Cyberpunk dla edytora Luce */
theme {
    name: "Cyberpunk 2077";
    background: #0f101d;
    foreground: #f4eee4;
    gutter-bg: #0b0c16;
    gutter-fg: #464b73;
    active-line: #1c1d30;
    cursor: #00ffcc;
    selection: #2e3052;
    sidebar-bg: #0b0c16;
    tab-bg: #0b0c16;
    tab-active-bg: #0f101d;
    tab-text: #464b73;
    tab-active-text: #00ffcc;
    statusbar-bg: #08090f;
    statusbar-fg: #f4eee4;
    terminal-bg: #0f101d;
    terminal-fg: #f4eee4;
    search-highlight: #ffee00;

    /* Tokeny Składni */
    syntax-keyword: #ff0055;
    syntax-type: #00e5ff;
    syntax-string: #fcee0a;
    syntax-character: #fcee0a;
    syntax-number: #ff7700;
    syntax-comment: #464b73;
    syntax-preprocessor: #ff0055;
    syntax-operator: #00ffcc;
    syntax-punctuation: #f4eee4;
    syntax-function: #00ff99;
    syntax-identifier: #f4eee4;
    syntax-namespace: #00e5ff;
    syntax-macro: #ff0055;
    syntax-attribute: #00ff99;
    syntax-tag: #ff0055;
    syntax-tag-bracket: #00ffcc;
    syntax-property: #00e5ff;
    syntax-value: #fcee0a;
    syntax-lifetime: #ff7700;
    syntax-escape: #ff0055;
}
```

---

## Tabela Dostępnych Tokenów

### 1. Elementy Interfejsu i Okna

| Klucz | Opis | Przykład |
| :--- | :--- | :--- |
| `name` | Wyświetlana nazwa w Command Palette | `"Mój Ciemny Motyw"` |
| `background` | Główne tło edytora i okna OpenGL | `#1e1e2e` |
| `foreground` | Domyślny kolor zwykłego tekstu | `#cdd6f4` |
| `gutter-bg` | Tło paska z numerami linii | `#181825` |
| `gutter-fg` | Kolor tekstu numerów linii | `#585b70` |
| `active-line` | Wyróżnienie aktywnej linii z kursorem | `#313244` |
| `cursor` | Kolor migającego kursora (karetki) | `#f5e0dc` |
| `selection` | Tło zaznaczonego tekstu | `#45475a` |
| `sidebar-bg` | Tło paska bocznego (drzewo plików) | `#181825` |
| `tab-bg` | Tło nieaktywnej karty | `#181825` |
| `tab-active-bg` | Tło aktywnej karty | `#1e1e2e` |
| `tab-text` | Kolor tekstu nieaktywnej karty | `#6c7086` |
| `tab-active-text` | Kolor tekstu aktywnej karty | `#cdd6f4` |
| `statusbar-bg` | Tło dolnego paska stanu | `#181825` |
| `statusbar-fg` | Kolor tekstu paska stanu | `#a6adc8` |
| `terminal-bg` | Tło wbudowanego terminala | `#11111b` |
| `terminal-fg` | Kolor tekstu wyjścia terminala | `#cdd6f4` |
| `search-highlight`| Kolor podświetlenia wyników szukania | `#f9e2af` |

---

## Przeładowanie na Żywo (Hot Reload)

1. Dokonaj zmian w pliku `themes/twoj_motyw.css` i zapisz plik (`Ctrl+S`).
2. Otwórz paletę poleceń (`Ctrl+Shift+P`).
3. Wpisz i uruchom:
   ```
   Theme: Reload Custom Themes
---

## Aby Dodać Nowy Motyw do Edytora Luce

1. Stwórz dowolny plik ze swoim motywem (np. `twoj_motyw.css`).
2. Umieść go w `sciezka-instalacji/themes/`.
3. Uruchom polecenie:
   ```
   Theme: Reload Custom Themes
   ```
4. Motyw zostanie automatycznie wykryty i będzie gotowy do użycia!