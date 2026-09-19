# Format on Save

Automatically strips unnecessary trailing whitespace from all lines whenever you save a file (`Ctrl+S`).

## Features

- **Automated Whitespace Cleanup**: Detects and trims trailing spaces and tabs on modified lines.
- **Save Hook Integration**: Hooks directly into Luce's `before_save` lifecycle event.
- **Non-destructive**: Keeps indentation and empty lines intact, only removing useless whitespace at line ends.
- **Log Feedback**: Reports number of cleaned lines to Luce's status log.

## Settings

Format on Save runs automatically on every active buffer during `before_save`.
