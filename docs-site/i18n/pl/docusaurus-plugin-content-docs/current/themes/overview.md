---
id: overview
title: Motywy i Palety Kolorów
sidebar_label: Przegląd
slug: /themes/overview
---

# Motywy i Palety Kolorów

Luce posiada rozbudowany silnik stylów, który pozwala dostosować kolory wszystkich elementów edytora — od tokenów składni po obramowania okien, karty, terminal oraz kolor tła okna OpenGL.

---

## Dostępne Motywy

Luce dostarczany jest z 4 wbudowanymi motywami oraz obsługą motywów CSS z folderu `themes/`:

1. **VS Code Dark 2026** *(Domyślny)*: Klasyczna, elegancka paleta Dark Modern wzorowana na Visual Studio Code (`#181818`).
2. **Catppuccin Mocha**: Ciepła, pastelowa paleta z charakterystycznym fioletem/lawendą mauve na pasku stanu.
3. **One Dark**: Zbalansowany ciemny motyw z błękitnymi akcentami Atom.
4. **Nord**: Mroźny, arktyczny motyw z arktycznym cyjanem na pasku stanu.
5. **Dracula** (`themes/dracula.css`): Fioletowy pasek slate purple z białym tekstem i żywą składnią.
6. **Cyberpunk 2077** (`themes/cyberpunk.css`): Ciemny neonowy fiolet z jarzącymi się cyjanowymi akcentami.

---

## Dynamiczne Kolorowanie Paska Stanu

Każdy motyw w pełni stylizuje dolny pasek statusu:
- `statusbar_bg`: Kolor tła dolnego paska stanu.
- `statusbar_fg`: Kolor tekstu dla gałęzi Git, języka, współrzędnych kursora (`Ln/Col`), kodowania, wcięć oraz wersji.
- Subtelna linia krawędzi (1px) dopasowana do odcienia motywu tworzy czyste oddzielenie edytora i terminala od paska.

---

## Zmiana Motywu

Motyw można zmienić w dowolnym momencie na trzy sposoby:

1. **Galeria w Ustawieniach (`Ctrl+,`)**:
   - Wejdź w **File -> Settings -> Appearance & Themes**.
   - Wybierz kartę motywu z interaktywnymi próbkami palety (w tym paska stanu).
2. **Command Palette (`Ctrl+Shift+P`)**:
   - Wpisz `Theme:` i naciśnij `Enter` na wybranym motywie.
3. **Menu główne**:
   - **View -> Theme -> Wybierz motyw z listy**.

Zmiany są natychmiast aplikowane w czasie rzeczywistym i trwale zapisywane w `settings.json`.
