---
id: source-control
title: Source Control (Git)
sidebar_label: Source Control (Git)
slug: /interface/source-control
---

# Source Control (Git)

Luce features native, enterprise-grade **Git** version control integration, allowing you to manage your complete code lifecycle directly from the editor without switching to an external terminal. From branch switching, staging, and commit management to stashing, tagging, remote management, repository cloning, and real-time command diagnostics, Luce provides a seamless, modern developer experience.

---

## 60 FPS Asynchronous Architecture & Optimistic Updates

Unlike many traditional editors, Luce's Source Control system is engineered on top of a **dedicated background worker thread** and **optimistic UI updates**:
- **Zero UI Freezes (0 ms latency)**: Staging (`git add`), unstaging (`git restore --staged`), discarding, or switching branches will never stutter or block the Dear ImGui 60/144 FPS render loop.
- **Optimistic Responsiveness**: Files move between staged and unstaged lists instantly (< 1 ms), while underlying Git filesystem tasks execute concurrently in the background and thread-safely reconcile their state upon completion.
- **Automatic Disk Reload**: Switching branches or running `Pull` automatically reloads the disk contents of all open clean editor tabs (`TabBar`), ensuring your active buffers are always in sync with your Git working tree.

---

## Source Control Panel

Open the Source Control panel by clicking the Git icon on the horizontal **Activity Bar** or pressing `Ctrl+Shift+P` and typing `View: Toggle Source Control`.

### Header & Quick Actions

- **Branch Dropdown (Combo)**: Displays the current active branch and allows instant switching to any local branch or launching the branch creation dialog.
- **New Branch (`+`)**: Opens the branch management modal.
- **Refresh (`↻`)**: Triggers an asynchronous background refresh of the index, remote tracking counters, and working tree.
- **More Actions (`···`)**: Opens a comprehensive, VS Code-styled menu containing all available Git operations.

---

## „More Actions” Menu (`···`)

The dropdown menu located in the top-right corner of the panel header provides a full hierarchy of Git commands:

```
··· (More Actions)
├── View as Tree (toggle file path display: filename (directory/path))
├── Pull (fetch and merge changes from upstream)
├── Push (publish and push local commits)
├── Clone... (clone repository from remote URL)
├── Checkout to... (switch to another local branch)
├── Fetch (download remote tracking refs)
├── Commit >
│   ├── Commit Staged (Amend)             [Requires confirmation]
│   └── Undo Last Commit (Soft)           [Requires confirmation]
├── Changes >
│   ├── Stage All Changes
│   ├── Unstage All Changes
│   └── Discard All Changes               [Requires confirmation]
├── Pull, Push >
│   ├── Pull
│   ├── Push
│   ├── Push (Force)                      [Requires confirmation]
│   └── Push (Tags)
├── Branch >
│   ├── Switch Branch...
│   ├── Create Branch...
│   ├── Rename Branch...
│   ├── Delete Branch...                  [Requires confirmation]
│   └── Merge Branch...                   [Requires confirmation]
├── Remote >
│   ├── Add Remote...
│   ├── Manage Remotes...                 [Removal requires confirmation]
│   └── Fetch (Prune)
├── Stash >
│   ├── Stash (Include Untracked)
│   ├── Stash (Keep Staged)
│   ├── Pop Latest Stash
│   ├── Apply Latest Stash
│   └── View / Manage Stashes...          [Drop requires confirmation]
├── Tags >
│   ├── Create Tag...
│   ├── Manage Tags...                    [Deletion requires confirmation]
│   └── Push Tags
└── Show Git Output                       [Real-time Git command log]
```

---

## Safety First: Centered Confirmation Modals

Before performing any **destructive or history-altering action**, Luce displays a centered, focused **Confirmation Modal Dialog**:
- The modal window is centered precisely in the middle of your screen and blocks the parent workspace from accidental input.
- Clearly states the consequences (e.g. permanent loss of uncommitted work).
- The action button (highlighted in red or amber) and the *Cancel* button each take 50% width, filling **100% of the bottom row** for ergonomic, balanced interaction.

### Actions Requiring Confirmation:
1. **Discard All Changes**: Discard all working tree modifications (`git restore .`).
2. **Discard Single File (`↺`)**: Discard local changes in a specific file.
3. **Force Push**: Overwrite remote branch history (`git push --force`).
4. **Delete Branch**: Delete a local branch (with an option for Force Delete `-D`).
5. **Merge Branch**: Merge another branch into the currently checked-out branch.
6. **Undo Last Commit (Soft)**: Reset HEAD by 1 commit while keeping all changes staged (`git reset --soft HEAD~1`).
7. **Commit Amend**: Amend previous commit with current staged changes (`git commit --amend`).
8. **Drop Stash**: Permanently discard a saved stash.
9. **Delete Tag**: Delete a version tag (`git tag -d`).
10. **Remove Remote**: Remove a configured remote repository.

---

## Dedicated Modals & Dialogs

All dialogs in Luce appear **dead-center** in your viewport with responsive layouts:

### 1. Branch Management (`Git Branches`)
- **Switch & Create**: Interactive filter box with real-time branch searching, one-click checkout, and a *Create New Branch* form (`git checkout -b`).
- **Rename**: Select any local branch from the dropdown and rename it (`git branch -m`).
- **Delete**: Select branch with optional *Force Delete (-D)* checkbox and safety confirmation.
- **Merge**: Select another branch to merge into your active branch (`git merge`).

### 2. Remote Management (`Git Remotes`)
- **Add Remote**: Input remote name (default `origin`) and repository URL (HTTPS or SSH).
- **Configured Remotes**: List of active remotes with their URLs and a *Remove* action.

### 3. Stash Management (`Git Stash`)
- **Save Stash**: Add an optional descriptive message, with options to `Include untracked (-u)` and `Keep index (keep staged)`.
- **Existing Stashes**: Scrollable list of `stash@{0}`, `stash@{1}`, etc., with actions:
  - **Apply**: Restore changes without removing the stash entry.
  - **Pop**: Apply changes and immediately delete the stash entry.
  - **Drop**: Permanently discard the stash entry (with confirmation).

### 4. Tag Management (`Git Tags`)
- **Create Tag**: Specify tag name (e.g. `v1.0.0`) and optional annotation message.
- **Existing Tags**: List of local tags with individual delete actions and a *Push All Tags to Remote* button (`git push --tags`).

### 5. Clone Repository (`Clone Repository`)
- Input repository URL and target destination.
- Includes a native folder picker button (**Browse...**).
- Once cloning completes, Luce automatically sets the directory as the active workspace, updating the File Explorer, embedded terminal, and Git status.

### 6. Git Output Console (`Git Output`)
- Real-time scrollable log of every Git command invoked by Luce.
- Shows timestamp `[HH:MM:SS]`, exact command arguments, exit status code (`[ok]` or `[exit code]`), and full stdout / stderr output.
- Features **Copy All** (copies entire session log to clipboard) and **Clear Output**.

---

## Synchronization: Push & Pull

Below the header, the panel offers dedicated Push and Pull buttons with real-time commit counter badges:
- **Push**: Shows local commits waiting to be pushed, e.g. `Push  ↑2`.
- **Pull**: Shows remote commits waiting to be pulled, e.g. `Pull  ↓1`.

:::tip Automatic Remote Setup
If your repository does not have an `origin` remote configured, clicking **Push** automatically opens the remote setup dialog and immediately pushes with tracking (`git push -u origin <branch>`).
:::

---

## Staging & Committing

1. **Commit Message**: Type your commit message into the input field and press **Ctrl+Enter** (or click *Commit*).
2. **Staged Changes**: View staged files with color-coded status badges (`A` – added, `M` – modified, `D` – deleted). Unstage individual files or all files at once.
3. **Changes**: View unstaged modifications. Click `+` to stage or `↺` to discard (with confirmation).
4. **View as Tree**: Toggle in the `···` menu to render file paths as `filename.cpp (path/to/folder)` for improved legibility.

---

## Git Gutter (Margin Indicators)

As you edit code, Luce automatically calculates line-by-line diffs against the repository `HEAD` and displays vibrant indicators on the left line-number margin:
- **Green Bar**: New lines added to the file.
- **Blue Bar**: Lines modified compared to the committed version.
- **Red Triangle**: Deleted lines marker positioned precisely where deletions occurred.
The markers refresh automatically in the background during active typing and upon file save.

---

## Visual Unified Diff Viewer

Click the **Diff** button next to any changed or staged file in the Source Control panel, or trigger `Git: View File Diff` from the Command Palette:
- **Syntax Highlighted Diff**: Additions (`+`) are highlighted in soft green with green backgrounds, deletions (`-`) in red with red backgrounds, and hunk headers (`@@`) in bright cyan.
- **File Header & Direct Actions**: View full file path, refresh the diff, or stage the file directly from within the modal window.

---

## Available Command Palette Entries (`Ctrl+Shift+P`)

All Git operations can be triggered from the Command Palette:

| Command Palette Entry | Command ID | Description |
|---|---|---|
| `View: Toggle Source Control` | `view.toggle_source_control` | Opens or closes the Git sidebar view |
| `Git: View File Diff` | `git.view_diff` | Opens the Visual Diff Viewer modal for the active file |
| `Git: Refresh Status` | `git.refresh` | Triggers an asynchronous status refresh |
| `Git: Switch / Checkout Branch...` | `git.branch.switch` | Opens the branch selection modal |
| `Git: Create New Branch...` | `git.branch.create` | Opens the new branch creation modal |
| `Git: Manage Branches...` | `git.branch.manage` | Opens the full branch management modal |
| `Git: Push` | `git.push` | Pushes local commits to the remote |
| `Git: Push (Force)` | `git.push_force` | Force pushes to the remote with confirmation |
| `Git: Pull` | `git.pull` | Pulls and merges changes from the remote |
| `Git: Add Remote Repository...` | `git.remote.add` | Opens the Add Remote dialog |
| `Git: Manage Remotes...` | `git.remote.manage` | Opens the remote repository manager |
| `Git: Stage All Changes` | `git.stage_all` | Stages all modified and untracked files |
| `Git: Unstage All Changes` | `git.unstage_all` | Unstages all files from the index |
| `Git: Discard All Changes` | `git.discard_all` | Discards all working tree changes with confirmation |
| `Git: Stash (Include Untracked)` | `git.stash.save` | Saves working tree changes to stash |
| `Git: Pop Latest Stash` | `git.stash.pop` | Applies and deletes the latest stash |
| `Git: Manage Stashes...` | `git.stash.manage` | Opens the stash manager modal |
| `Git: Manage Tags...` | `git.tag.manage` | Opens the tag manager modal |
| `Git: Clone Repository...` | `git.clone` | Opens the Clone Repository wizard |
| `Git: Show Git Output Log` | `git.output` | Opens the real-time Git Output log |
