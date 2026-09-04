---
id: source-control
title: Kontrola Wersji (Git)
sidebar_label: Kontrola Wersji (Git)
slug: /interface/source-control
---

# Kontrola Wersji (Git)

Luce posiada natywną, wbudowaną integrację z systemem **Git**, umożliwiającą pełne zarządzanie cyklem życia kodu bezpośrednio z poziomu edytora bez potrzeby ciągłego przełączania się do zewnętrznej konsoli.

---

## Panel Source Control

Dostęp do panelu kontroli wersji uzyskasz poprzez kliknięcie ikony Git w poziomym **Activity Barze** lub za pomocą polecenia `View: Toggle Source Control` w Command Palette (`Ctrl+Shift+P`).

### Główne Funkcje

1. **Informacja o Gałęzi i Przycisk Odświeżania**:
   - W górnej części panelu wyświetlana jest nazwa aktualnie wybranej gałęzi (`Branch: <nazwa>`).
   - Obok nazwy znajduje się dedykowany przycisk odświeżenia statusu (ikona `↻`), który ponownie bada stan drzewa roboczego i indeksu Git.
2. **Commitowanie Zmian**:
   - Pole tekstowe *Message* pozwala na szybkie wpisanie wiadomości commita.
   - Skrót **Ctrl+Enter** (lub przycisk *Commit*) natychmiast zatwierdza przygotowane zmiany (`git commit -m "..."`).
3. **Staged Changes (Przygotowane Zmiany)**:
   - Lista plików dodanych do indeksu (`git add`).
   - Każdy plik posiada oznaczony status: `A` (dodany), `M` (zmodyfikowany), `D` (usunięty).
   - Przycisk `-` przy pliku pozwala na wycofanie go z indeksu (`git reset HEAD <plik>`), a przycisk w nagłówku sekcji wycofuje wszystkie pliki na raz.
4. **Changes (Zmiany Robocze)**:
   - Lista plików zmodyfikowanych w katalogu roboczym, lecz jeszcze nieprzygotowanych do commita.
   - Przycisk `+` przygotowuje pojedynczy plik lub całe repozytorium.
   - Przycisk `↺` (*Discard Changes*) cofa lokalne modyfikacje pliku do stanu z ostatniego commita.
5. **Szybka Nawigacja**:
   - Kliknięcie w dowolny plik z listy zmian natychmiast otwiera go w aktywnej karcie edytora.

---

## Oznaczenia Statusu w Eksploratorze Plików

Stan repozytorium Git jest automatycznie odzwierciedlany w drzewie plików:
- **`M` (Modified)** — żółte podświetlenie nazwy pliku ze znacznikiem `M` po prawej stronie.
- **`U` (Untracked)** — zielone podświetlenie dla nowych, nieśledzonych plików ze znacznikiem `U`.
- **`D` (Deleted)** — czerwone podświetlenie dla usuniętych plików ze znacznikiem `D`.

Dodatkowo aktualna gałąź Git (`git: <branch>`) jest stale widoczna w lewym dolnym rogu na pasku stanu edytora.

---

## Dostępne Komendy w Palecie

| Komenda w Command Palette | Identyfikator | Opis |
|---|---|---|
| `View: Toggle Source Control` | `view.toggle_source_control` | Otwiera lub zamyka panel Git w pasku bocznym |
| `Git: Refresh Status` | `git.refresh` | Wymusza odświeżenie statusu repozytorium |
| `Git: Stage All Changes` | `git.stage_all` | Dodaje wszystkie zmodyfikowane i nowe pliki do indeksu |
| `Git: Unstage All Changes` | `git.unstage_all` | Wycofuje wszystkie pliki z indeksu |
