---
id: source-control
title: Kontrola Wersji (Git)
sidebar_label: Kontrola Wersji (Git)
slug: /interface/source-control
---

# Kontrola Wersji (Git)

Luce posiada zaawansowaną, wbudowaną integrację z systemem kontroli wersji **Git**. Pozwala ona na pełne zarządzanie cyklem życia kodu — od tworzenia gałęzi i commitowania, przez zarządzanie schowkiem (stash), tagami i serwerami zdalnymi (remotes), aż po klonowanie repozytoriów i diagnostykę poleceń — bezpośrednio z poziomu edytora.

---

## Architektura Asynchroniczna

W odróżnieniu od wielu tradycyjnych edytorów, panel Source Control w Luce oparty jest na **dedykowanym wątku roboczym w tle (Worker Thread)** oraz mechanizmie **optymistycznych aktualizacji interfejsu (Optimistic UI Updates)**:

- **Optymistyczna synchronizacja**: Zmiany w listach plików wyświetlają się natychmiastowo, podczas gdy operacje dyskowe Git wykonują się w tle, a ich wynik jest płynnie i bezpiecznie integrowany po zakończeniu.
- **Automatyczne przeładowywanie z dysku**: Przełączenie gałęzi lub operacja `Pull` automatycznie odświeża zawartość wszystkich otwartych w edytorze kart (`TabBar`), zapobiegając nadpisaniu niespójnych danych.

---

## Panel Source Control

Panel kontroli wersji otworzysz, klikając ikonę Git w **Activity Barze** lub za pomocą skrótu z Command Palette: `View: Toggle Source Control`.

### Nagłówek i Szybkie Akcje

- **Przełącznik gałęzi (Branch Selector)**: Rozwijana lista prezentuje bieżącą gałąź i pozwala na natychmiastowe przełączenie się na inną lokalną gałąź lub otwarcie kreatora.
- **Nowa gałąź (`+`)**: Szybkie przejście do modalnego okna zarządzania gałęziami.
- **Odświeżenie (`↻`)**: Wymusza asynchroniczne przeskanowanie indeksu oraz drzewa roboczego.
- **Menu Więcej Akcji (`···`)**: Otwiera kompleksowe menu wszystkich dostępnych operacji Git.

---

## Menu „Więcej Akcji” (`···`)

Rozwijane menu w prawym górnym rogu panelu oferuje kompletną hierarchię narzędzi:

```
··· (Więcej Akcji)
├── View as Tree (przełączanie widoku ścieżek: nazwa_pliku (katalog))
├── Pull (pobranie i scalenie zmian ze zdalnego repozytorium)
├── Push (wypchnięcie lokalnych commitów)
├── Clone... (kreator klonowania nowego repozytorium)
├── Checkout to... (szybkie przełączenie gałęzi)
├── Fetch (pobranie najnowszych referencji)
├── Commit >
│   ├── Commit Staged (Amend)             [Wymaga potwierdzenia]
│   └── Undo Last Commit (Soft)           [Wymaga potwierdzenia]
├── Changes >
│   ├── Stage All Changes
│   ├── Unstage All Changes
│   └── Discard All Changes               [Wymaga potwierdzenia]
├── Pull, Push >
│   ├── Pull
│   ├── Push
│   ├── Push (Force)                      [Wymaga potwierdzenia]
│   └── Push (Tags)
├── Branch >
│   ├── Switch Branch...
│   ├── Create Branch...
│   ├── Rename Branch...
│   ├── Delete Branch...                  [Wymaga potwierdzenia]
│   └── Merge Branch...                   [Wymaga potwierdzenia]
├── Remote >
│   ├── Add Remote...
│   ├── Manage Remotes...                 [Usuwanie wymaga potwierdzenia]
│   └── Fetch (Prune)
├── Stash >
│   ├── Stash (Include Untracked)
│   ├── Stash (Keep Staged)
│   ├── Pop Latest Stash
│   ├── Apply Latest Stash
│   └── View / Manage Stashes...          [Usuwanie wymaga potwierdzenia]
├── Tags >
│   ├── Create Tag...
│   ├── Manage Tags...                    [Usuwanie wymaga potwierdzenia]
│   └── Push Tags
└── Show Git Output                       [Konsola poleceń Git na żywo]
```

---

## Bezpieczeństwo: Modalne Potwierdzenia Akcji

Przed wykonaniem jakiejkolwiek **potencjalnie niszczącej akcji** lub modyfikacji historii repozytorium edytor wyświetla wycentrowane **modalne okno potwierdzenia** (*Confirm Action Modal*):
- Okno blokuje interfejs, uniemożliwiając przypadkowe kliknięcie w tle.
- Wyraźnie opisuje konsekwencje operacji (np. nieodwracalną utratę niezacommitowanych danych w plikach).
- Przycisk akcji (w kolorze ostrzegawczym/czerwonym) oraz przycisk *Cancel* mają zbalansowaną szerokość i wypełniają 100% dolnego paska okna.

### Akcje objęte potwierdzeniem:
1. **Discard All Changes**: Odrzucenie wszystkich zmian roboczych (`git restore .`).
2. **Discard Single File (`↺`)**: Odrzucenie zmian w pojedynczym pliku.
3. **Force Push**: Wymuszone nadpisanie historii zdalnej gałęzi (`git push --force`).
4. **Delete Branch**: Usunięcie gałęzi (w tym wymuszone usunięcie `-D` niescalonych commitów).
5. **Merge Branch**: Scalenie innej gałęzi do bieżącej.
6. **Undo Last Commit (Soft)**: Cofnięcie ostatniego commita z zachowaniem zmian w indeksie (`git reset --soft HEAD~1`).
7. **Commit Amend**: Nadpisanie poprzedniego commita bieżącymi zmianami.
8. **Drop Stash**: Trwałe usunięcie schowka stash.
9. **Delete Tag**: Usunięcie tagu wersji.
10. **Remove Remote**: Usunięcie skonfigurowanego serwera zdalnego.

---

## Zaawansowane Okna Dialogowe (Modals)

Wszystkie okna dialogowe w Luce pojawiają się **dokładnie na środku ekranu** i są w pełni responsywne.

### 1. Zarządzanie Gałęziami (`Git Branches`)
Dostępne po kliknięciu nazwy gałęzi lub z menu:
- **Switch & Create**: Szybka wyszukiwarka/filtr gałęzi z natychmiastowym przełączaniem oraz formularz tworzenia nowej gałęzi (`git checkout -b`).
- **Rename**: Wybór gałęzi z listy rozwijanej i zmiana jej nazwy (`git branch -m`).
- **Delete**: Bezpieczne usuwanie gałęzi z opcjonalnym przełącznikiem *Force Delete (-D)*.
- **Merge**: Wybór gałęzi do scalenia z aktualnie aktywną gałęzią roboczą (`git merge`).

### 2. Zarządzanie Serwerami Zdalnymi (`Git Remotes`)
- Formularz dodawania nowego serwera: nazwa (domyślnie `origin`) oraz adres URL (HTTPS/SSH).
- Przeglądanie listy podpiętych serwerów zdalnych wraz z ich adresami oraz opcją bezpiecznego usunięcia.

### 3. Zarządzanie Schowkiem (`Git Stash`)
- **Tworzenie schowka**: Pole na opcjonalny opis oraz przełączniki:
  - `Include untracked (-u)` — dołącza do schowka nowe, nieśledzone pliki.
  - `Keep index` — zachowuje pliki przygotowane w indeksie (staged).
- **Lista schowków**: Przeglądanie zapisanych stanów (`stash@{0}`, `stash@{1}`) z możliwością:
  - **Apply**: Przywrócenie zmian bez usuwania schowka.
  - **Pop**: Przywrócenie zmian i jednoczesne usunięcie wpisu ze schowka.
  - **Drop**: Trwałe skasowanie schowka (z potwierdzeniem).

### 4. Tagi Wersji (`Git Tags`)
- **Tworzenie tagu**: Nazwa tagu (np. `v1.2.0`) oraz opcjonalna wiadomość annotacji.
- **Lista tagów**: Przeglądanie tagów, ich usuwanie oraz przycisk *Push All Tags to Remote* do wysłania wszystkich tagów na serwer zdalny.

### 5. Klonowanie Repozytorium (`Clone Repository`)
- Formularz klonowania:
  - Adres URL repozytorium (np. `https://github.com/user/project.git`).
  - Ścieżka docelowa wraz z natywnym selektorem folderów systemowych (**Browse...**).
- Po pomyślnym sklonowaniu edytor automatycznie konfiguruje i otwiera pobrane repozytorium jako aktywny obszar roboczy (aktualizując Eksplorator Plików, wbudowany terminal oraz panel Git).

### 6. Podgląd Działań Git (`Git Output`)
- Wbudowane okno inspekcji prezentujące pełną historię poleceń Git uruchamianych w tle przez Luce.
- Każdy wpis zawiera:
  - Dokładny znacznik czasu `[GG:MM:SS]`.
  - Treść polecenia (np. `git status --porcelain=v1 -uall`).
  - Status wykonania: `[ok]` lub `[exit code]`.
  - Pełną treść zwróconą przez Git (stdout / stderr).
- Przyciski **Copy All** (kopiowanie całego logu do schowka) oraz **Clear Output**.

---

## Synchronizacja: Push & Pull

Poniżej nagłówka panelu znajdują się dedykowane przyciski synchronizacji z dynamicznymi licznikami:
- **Push**: Wyświetla liczbę lokalnych commitów oczekujących na wypchnięcie, np. `Push  ↑2`.
- **Pull**: Wyświetla liczbę commitów dostępnych na serwerze do pobrania, np. `Pull  ↓1`.

:::tip Automatyczny Kreator Remote
Jeśli repozytorium nie ma jeszcze skonfigurowanego serwera zdalnego `origin`, kliknięcie przycisku **Push** automatycznie otworzy okno konfiguracji serwera zdalnego i po podaniu adresu natychmiast wypchnie gałąź z powiązaniem nadrzędnym (`git push -u origin <branch>`).
:::

---

## Przygotowywanie Zmian i Commitowanie

1. **Commit Message**: Wpisz wiadomość w polu tekstowym i użyj skrótu **Ctrl+Enter** (lub przycisku *Commit*).
2. **Staged Changes**: Lista plików w indeksie z oznaczeniami kolorystycznymi (`A` – dodany, `M` – zmodyfikowany, `D` – usunięty).
3. **Changes**: Lista plików w katalogu roboczym. Kliknięcie `+` przenosi do indeksu, a kliknięcie `↺` cofa zmiany po potwierdzeniu.
4. **View as Tree**: Włączenie tej opcji w menu `···` zmienia format wyświetlania plików na bardziej czytelny: `plik.cpp (src/editor)`.

---

## Dostępne Komendy w Palecie Poleceń (`Ctrl+Shift+P`)

Wszystkie operacje Git można wywołać bezpośrednio z Command Palette:

| Komenda w Command Palette | Identyfikator | Opis |
|---|---|---|
| `View: Toggle Source Control` | `view.toggle_source_control` | Otwiera lub ukrywa panel Source Control |
| `Git: Refresh Status` | `git.refresh` | Wymusza asynchroniczne odświeżenie statusu |
| `Git: Switch / Checkout Branch...` | `git.branch.switch` | Otwiera okno wyboru i przełączania gałęzi |
| `Git: Create New Branch...` | `git.branch.create` | Otwiera kreator tworzenia nowej gałęzi |
| `Git: Manage Branches...` | `git.branch.manage` | Otwiera pełne okno gałęzi (zmiana nazwy, usuwanie, scalanie) |
| `Git: Push` | `git.push` | Wypycha commity na serwer zdalny |
| `Git: Push (Force)` | `git.push_force` | Wymuszone wypchnięcie zmian z potwierdzeniem |
| `Git: Pull` | `git.pull` | Pobiera i scala zmiany z serwera zdalnego |
| `Git: Add Remote Repository...` | `git.remote.add` | Otwiera okno dodawania serwera zdalnego |
| `Git: Manage Remotes...` | `git.remote.manage` | Otwiera okno zarządzania serwerami zdalnymi |
| `Git: Stage All Changes` | `git.stage_all` | Dodaje wszystkie zmodyfikowane pliki do indeksu |
| `Git: Unstage All Changes` | `git.unstage_all` | Wycofuje wszystkie pliki z indeksu |
| `Git: Discard All Changes` | `git.discard_all` | Odrzuca wszystkie niezacommitowane zmiany (z potwierdzeniem) |
| `Git: Stash (Include Untracked)` | `git.stash.save` | Zapisuje zmiany robocze w schowku |
| `Git: Pop Latest Stash` | `git.stash.pop` | Przywraca i usuwa ostatni schowek |
| `Git: Manage Stashes...` | `git.stash.manage` | Otwiera okno zarządzania schowkami |
| `Git: Manage Tags...` | `git.tag.manage` | Otwiera okno zarządzania tagami |
| `Git: Clone Repository...` | `git.clone` | Otwiera kreator klonowania repozytorium |
| `Git: Show Git Output Log` | `git.output` | Otwiera okno podglądu logów Git |
