---
id: source-control
title: Source Control (Git)
sidebar_label: Source Control (Git)
slug: /interface/source-control
---

# Source Control (Git)

Luce features native, built-in **Git** version control integration, allowing you to manage your code repository lifecycle directly from the editor without toggling to an external terminal.

---

## Source Control Panel

Access the source control panel by clicking the Git icon on the horizontal **Activity Bar** or running `View: Toggle Source Control` via the Command Palette (`Ctrl+Shift+P`).

### Key Features

1. **Branch Indicator & Refresh Action**:
   - The top header displays the active Git branch (`Branch: <name>`).
   - A right-aligned refresh button (circular reload icon `↻`) re-scans the repository index and working tree.
2. **Commit Changes**:
   - The commit message box lets you draft commit notes quickly.
   - Press **Ctrl+Enter** (or click the *Commit* button) to stage and commit staged changes immediately (`git commit -m "..."`).
3. **Staged Changes**:
   - Lists files prepared in the index (`git add`).
   - File status is indicated via color-coded badges: `A` (added), `M` (modified), `D` (deleted).
   - Click `-` on any file to unstage it (`git reset HEAD <file>`), or use the header button to unstage all.
4. **Changes (Working Tree)**:
   - Lists unstaged modifications in the workspace.
   - Click `+` to stage individual files or all changes.
   - Click `↺` (*Discard Changes*) to restore the file to the last committed state.
5. **Instant File Navigation**:
   - Clicking any item in the change list immediately opens it in the active editor tab.

---

## File Explorer Status Badges

Repository status is reflected across the File Explorer tree:
- **`M` (Modified)** — Yellow highlight with an `M` badge on the right.
- **`U` (Untracked)** — Green highlight for new files with a `U` badge.
- **`D` (Deleted)** — Red highlight for deleted files with a `D` badge.

Additionally, the active Git branch (`git: <branch>`) is persistently shown on the left side of the bottom status bar.

---

## Available Commands

| Command Palette Entry | Command ID | Description |
|---|---|---|
| `View: Toggle Source Control` | `view.toggle_source_control` | Opens or closes the Git sidebar view |
| `Git: Refresh Status` | `git.refresh` | Forces a repository status refresh |
| `Git: Stage All Changes` | `git.stage_all` | Stages all modified and untracked files |
| `Git: Unstage All Changes` | `git.unstage_all` | Unstages all files from the index |
