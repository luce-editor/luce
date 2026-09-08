---
id: creating-themes
title: Creating Custom Themes
sidebar_label: Custom Themes (CSS)
slug: /themes/creating-themes
---

# Creating Custom Themes with CSS

Luce supports creating custom themes using a simple, human-friendly CSS-like format.

Custom themes are loaded dynamically from `.css` or `.lucetheme` files placed inside the `themes/` directory.

---

## File Format & Structure

Create a file named `themes/my_theme.css`:

```css
/* Cyberpunk Neon Theme for Luce Editor */
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

    /* Syntax Tokens */
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

## Supported Token Reference Table

### 1. Editor Chrome & UI Tokens

| Key | Description | Example |
| :--- | :--- | :--- |
| `name` | Human-readable name shown in Command Palette | `"My Dark Theme"` |
| `background` | Primary editor and window background | `#1e1e2e` |
| `foreground` | Default unhighlighted text color | `#cdd6f4` |
| `gutter-bg` | Line numbers column background | `#181825` |
| `gutter-fg` | Line numbers text color | `#585b70` |
| `active-line` | Highlight tint for the line currently holding cursor | `#313244` |
| `cursor` | Caret blinking line color | `#f5e0dc` |
| `selection` | Selection background for highlighted text blocks | `#45475a` |
| `sidebar-bg` | Left sidebar panel background | `#181825` |
| `tab-bg` | Inactive tab header background | `#181825` |
| `tab-active-bg` | Active tab header background | `#1e1e2e` |
| `tab-text` | Inactive tab label color | `#6c7086` |
| `tab-active-text` | Active tab label color | `#cdd6f4` |
| `statusbar-bg` | Bottom status bar background | `#181825` |
| `statusbar-fg` | Bottom status bar text color | `#a6adc8` |
| `terminal-bg` | Embedded terminal console background | `#11111b` |
| `terminal-fg` | Embedded terminal text output color | `#cdd6f4` |
| `search-highlight`| Highlighting for search match results | `#f9e2af` |

### 2. Syntax Highlighting Tokens

| Key | Maps to TokenType | Examples |
| :--- | :--- | :--- |
| `syntax-keyword` | `TokenType::Keyword` | `if`, `else`, `while`, `return`, `struct`, `class` |
| `syntax-type` | `TokenType::Type` | `int`, `float`, `bool`, `std::string`, `uint32_t` |
| `syntax-string` | `TokenType::String` | `"hello world"` |
| `syntax-character` | `TokenType::Character` | `'c'`, `'\n'` |
| `syntax-number` | `TokenType::Number` | `123`, `0xFF`, `3.1415f` |
| `syntax-comment` | `TokenType::Comment` | `// inline comment`, `/* block */` |
| `syntax-preprocessor` | `TokenType::Preprocessor` | `#include`, `#define`, `#pragma` |
| `syntax-operator` | `TokenType::Operator` | `+`, `-`, `*`, `&`, `|`, `->` |
| `syntax-punctuation` | `TokenType::Punctuation` | `;`, `,`, `(`, `)` |
| `syntax-function` | `TokenType::Function` | `printf()`, `std::make_unique()` |
| `syntax-identifier`| `TokenType::Identifier`| Local variables, member fields |
| `syntax-namespace` | `TokenType::Namespace` | `std`, `luce`, `boost` |
| `syntax-macro` | `TokenType::Macro` | `DEBUG_ASSERT`, `MAKE_HEX` |
| `syntax-attribute` | `TokenType::Attribute` | `[[nodiscard]]`, `[[maybe_unused]]` |
| `syntax-tag` | `TokenType::Tag` | HTML/XML tags: `<div>`, `<header>` |
| `syntax-tag-bracket`| `TokenType::TagBracket` | `<`, `>`, `</` |
| `syntax-property` | `TokenType::Property` | CSS properties: `margin`, `color` |
| `syntax-value` | `TokenType::Value` | CSS values: `auto`, `none`, `flex` |
| `syntax-lifetime` | `TokenType::Lifetime` | Rust lifetimes: `'a`, `'static` |
| `syntax-escape` | `TokenType::Escape` | String escape sequences: `\n`, `\t`, `\x41` |

---

## Live Hot Reloading

When developing a theme in Luce:

1. Open `themes/your_theme.css` in Luce.
2. Edit colors and save the file (`Ctrl+S`).
3. Press `Ctrl+Shift+P` and execute:
   ```
   Theme: Reload Custom Themes
   ```
4. Luce will re-parse all theme files in `themes/` and update the active theme instantly without restarting!

---

## How to Add a New Theme to Luce Editor

1. Create a file containing your custom theme definitions (e.g. `my_theme.css`).
2. Place it in `installation-path/themes/`.
3. Run the command:
   ```
   Theme: Reload Custom Themes
   ```
4. The theme will be automatically detected and ready to use!
