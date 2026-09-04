// ============================================================================
// Theme — Implementation.
//
// Built-in themes: VSCode Dark (2026 Dark Modern Default), Catppuccin Mocha, One Dark, Nord.
// Custom themes: loaded dynamically from .css and .lucetheme files in themes/
// ============================================================================

#include "theme.h"
#include "platform.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

namespace fs = std::filesystem;

namespace luce {

// ── Helper: construct ImVec4 from 0x RRGGBB hex (alpha = 1.0) ─────────────
static ImVec4 Hex(unsigned int rgb, float a = 1.0f) {
    return {
        ((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >> 8)  & 0xFF) / 255.0f,
        ((rgb)       & 0xFF) / 255.0f,
        a
    };
}

// ── Helper: linear blend between two colours (t=0 → a, t=1 → b) ───────────
static ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}

// ── Helper: copy a colour with a new alpha ────────────────────────────────
static ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }

static std::optional<ImVec4> ParseCSSColor(std::string str) {
    // trim whitespace and quotes and trailing semicolon
    while (!str.empty() && (std::isspace(static_cast<unsigned char>(str.front())) || str.front() == '"' || str.front() == '\'')) str.erase(str.begin());
    while (!str.empty() && (std::isspace(static_cast<unsigned char>(str.back())) || str.back() == '"' || str.back() == '\'' || str.back() == ';')) str.pop_back();

    if (str.empty()) return std::nullopt;
    if (str.front() == '#') str.erase(str.begin());

    // Check if 3-hex (#RGB -> #RRGGBB)
    if (str.size() == 3) {
        std::string expanded;
        expanded += str[0]; expanded += str[0];
        expanded += str[1]; expanded += str[1];
        expanded += str[2]; expanded += str[2];
        str = expanded;
    }

    if (str.size() == 6 || str.size() == 8) {
        try {
            unsigned int val = static_cast<unsigned int>(std::stoul(str, nullptr, 16));
            if (str.size() == 6) {
                return Hex(val, 1.0f);
            } else {
                float a = (val & 0xFF) / 255.0f;
                return Hex(val >> 8, a);
            }
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

ImVec4 Theme::GetTokenColor(TokenType tt) const {
    switch (tt) {
        case TokenType::Keyword:      return syntax_keyword;
        case TokenType::Type:         return syntax_type;
        case TokenType::String:       return syntax_string;
        case TokenType::Character:    return syntax_character;
        case TokenType::Number:       return syntax_number;
        case TokenType::Comment:      return syntax_comment;
        case TokenType::Preprocessor: return syntax_preprocessor;
        case TokenType::Operator:     return syntax_operator;
        case TokenType::Punctuation:  return syntax_punctuation;
        case TokenType::Function:     return syntax_function;
        case TokenType::Identifier:   return syntax_identifier;
        case TokenType::Namespace:    return syntax_namespace;
        case TokenType::Macro:        return syntax_macro;
        case TokenType::Attribute:    return syntax_attribute;
        case TokenType::Tag:          return syntax_tag;
        case TokenType::TagBracket:   return syntax_tag_bracket;
        case TokenType::Property:     return syntax_property;
        case TokenType::Value:        return syntax_value;
        case TokenType::Lifetime:     return syntax_lifetime;
        case TokenType::Escape:       return syntax_escape;
        default:                      return foreground;
    }
}

// ── Theme definitions ─────────────────────────────────────────────────────

/// VS Code Dark Modern 2026 (Default)
static Theme MakeVSCodeDark2026() {
    Theme t;
    t.name = "VS Code Dark 2026";

    // UI — exact VS Code Dark Modern hex codes
    t.background      = Hex(0x181818);  // Editor background
    t.foreground       = Hex(0xcccccc);  // Editor text
    t.gutter_bg        = Hex(0x181818);  // Gutter bg matches editor
    t.gutter_fg        = Hex(0x6e7681);  // Line numbers
    t.active_line      = Hex(0x282828, 0.5f); // Active line highlight
    t.cursor_color     = Hex(0x0078d4);  // Blue cursor
    t.selection        = Hex(0x264f78, 0.8f); // Selection blue
    t.sidebar_bg       = Hex(0x181818);  // Sidebar background
    t.tab_bg           = Hex(0x181818);  // Inactive tab
    t.tab_active_bg    = Hex(0x1f1f1f);  // Active tab
    t.tab_text         = Hex(0x8c8c8c);  // Inactive tab text
    t.tab_active_text  = Hex(0xffffff);  // Active tab text
    t.statusbar_bg     = Hex(0x181818);  // Status bar
    t.statusbar_fg     = Hex(0xcccccc);  // Status bar text
    t.terminal_bg      = Hex(0x181818);  // Terminal bg
    t.terminal_fg      = Hex(0xcccccc);  // Terminal fg
    t.search_highlight = Hex(0x515c6b, 0.7f);

    // Syntax Highlighting (Dark Modern / C++ / Web / Rust)
    t.syntax_keyword      = Hex(0x569cd6);  // Blue keywords (const, auto, struct, if)
    t.syntax_type          = Hex(0x4ec9b0);  // Teal / Cyan types (int, String, App)
    t.syntax_string        = Hex(0xce9178);  // Orange / Salmon strings
    t.syntax_character     = Hex(0xce9178);
    t.syntax_number        = Hex(0xb5cea8);  // Light olive numbers (0, 42, 3.14)
    t.syntax_comment       = Hex(0x6a9955);  // Green comments (// ...)
    t.syntax_preprocessor  = Hex(0xc586c0);  // Purple / Magenta preprocessor (#include, #define)
    t.syntax_operator      = Hex(0xd4d4d4);  // Neutral operators (+ - =)
    t.syntax_punctuation   = Hex(0xcccccc);  // Brackets and dots
    t.syntax_function      = Hex(0xdcdcaa);  // Yellow functions (MakeCheckboxList, render)
    t.syntax_identifier    = Hex(0x9cdcfe);  // Light blue variables / params (parent_id)
    t.syntax_namespace     = Hex(0x4ec9b0);
    t.syntax_macro         = Hex(0xc586c0);
    t.syntax_attribute     = Hex(0x4ec9b0);
    t.syntax_tag           = Hex(0x569cd6);
    t.syntax_tag_bracket   = Hex(0x808080);
    t.syntax_property      = Hex(0x9cdcfe);
    t.syntax_value         = Hex(0xce9178);
    t.syntax_lifetime      = Hex(0x569cd6);
    t.syntax_escape        = Hex(0xd7ba7d);

    return t;
}

/// Catppuccin Mocha
static Theme MakeCatppuccinMocha() {
    Theme t;
    t.name = "Catppuccin Mocha";

    // UI
    t.background      = Hex(0x1e1e2e);
    t.foreground       = Hex(0xcdd6f4);
    t.gutter_bg        = Hex(0x181825);
    t.gutter_fg        = Hex(0x585b70);
    t.active_line      = Hex(0x313244, 0.6f);
    t.cursor_color     = Hex(0xf5e0dc);
    t.selection        = Hex(0x45475a, 0.55f);
    t.sidebar_bg       = Hex(0x181825);
    t.tab_bg           = Hex(0x181825);
    t.tab_active_bg    = Hex(0x1e1e2e);
    t.tab_text         = Hex(0x6c7086);
    t.tab_active_text  = Hex(0xcdd6f4);
    t.statusbar_bg     = Hex(0x181825);
    t.statusbar_fg     = Hex(0xa6adc8);
    t.terminal_bg      = Hex(0x11111b);
    t.terminal_fg      = Hex(0xcdd6f4);
    t.search_highlight = Hex(0xf9e2af, 0.35f);

    // Syntax
    t.syntax_keyword      = Hex(0xcba6f7);
    t.syntax_type          = Hex(0xf9e2af);
    t.syntax_string        = Hex(0xa6e3a1);
    t.syntax_character     = Hex(0xa6e3a1);
    t.syntax_number        = Hex(0xfab387);
    t.syntax_comment       = Hex(0x6c7086);
    t.syntax_preprocessor  = Hex(0xf38ba8);
    t.syntax_operator      = Hex(0x89dceb);
    t.syntax_punctuation   = Hex(0x9399b2);
    t.syntax_function      = Hex(0x89b4fa);
    t.syntax_identifier    = Hex(0xcdd6f4);
    t.syntax_namespace     = Hex(0xf9e2af);
    t.syntax_macro         = Hex(0x94e2d5);
    t.syntax_attribute     = Hex(0xf9e2af);
    t.syntax_tag           = Hex(0xf38ba8);
    t.syntax_tag_bracket   = Hex(0x9399b2);
    t.syntax_property      = Hex(0x89b4fa);
    t.syntax_value         = Hex(0xa6e3a1);
    t.syntax_lifetime      = Hex(0xf38ba8);
    t.syntax_escape        = Hex(0xfab387);

    return t;
}

/// One Dark
static Theme MakeOneDark() {
    Theme t;
    t.name = "One Dark";

    t.background      = Hex(0x282c34);
    t.foreground       = Hex(0xabb2bf);
    t.gutter_bg        = Hex(0x282c34);
    t.gutter_fg        = Hex(0x4b5263);
    t.active_line      = Hex(0x2c313c, 0.7f);
    t.cursor_color     = Hex(0x528bff);
    t.selection        = Hex(0x3e4451, 0.6f);
    t.sidebar_bg       = Hex(0x21252b);
    t.tab_bg           = Hex(0x21252b);
    t.tab_active_bg    = Hex(0x282c34);
    t.tab_text         = Hex(0x5c6370);
    t.tab_active_text  = Hex(0xabb2bf);
    t.statusbar_bg     = Hex(0x21252b);
    t.statusbar_fg     = Hex(0x9da5b4);
    t.terminal_bg      = Hex(0x1e2127);
    t.terminal_fg      = Hex(0xabb2bf);
    t.search_highlight = Hex(0xe5c07b, 0.35f);

    t.syntax_keyword      = Hex(0xc678dd);
    t.syntax_type          = Hex(0xe5c07b);
    t.syntax_string        = Hex(0x98c379);
    t.syntax_character     = Hex(0x98c379);
    t.syntax_number        = Hex(0xd19a66);
    t.syntax_comment       = Hex(0x5c6370);
    t.syntax_preprocessor  = Hex(0xe06c75);
    t.syntax_operator      = Hex(0x56b6c2);
    t.syntax_punctuation   = Hex(0xabb2bf);
    t.syntax_function      = Hex(0x61afef);
    t.syntax_identifier    = Hex(0xabb2bf);
    t.syntax_namespace     = Hex(0xe5c07b);
    t.syntax_macro         = Hex(0x56b6c2);
    t.syntax_attribute     = Hex(0xe5c07b);
    t.syntax_tag           = Hex(0xe06c75);
    t.syntax_tag_bracket   = Hex(0xabb2bf);
    t.syntax_property      = Hex(0x61afef);
    t.syntax_value         = Hex(0x98c379);
    t.syntax_lifetime      = Hex(0xe06c75);
    t.syntax_escape        = Hex(0xd19a66);

    return t;
}

/// Nord
static Theme MakeNord() {
    Theme t;
    t.name = "Nord";

    t.background      = Hex(0x2e3440);
    t.foreground       = Hex(0xd8dee9);
    t.gutter_bg        = Hex(0x2e3440);
    t.gutter_fg        = Hex(0x4c566a);
    t.active_line      = Hex(0x3b4252, 0.7f);
    t.cursor_color     = Hex(0x88c0d0);
    t.selection        = Hex(0x434c5e, 0.6f);
    t.sidebar_bg       = Hex(0x2e3440);
    t.tab_bg           = Hex(0x2e3440);
    t.tab_active_bg    = Hex(0x3b4252);
    t.tab_text         = Hex(0x4c566a);
    t.tab_active_text  = Hex(0xeceff4);
    t.statusbar_bg     = Hex(0x3b4252);
    t.statusbar_fg     = Hex(0xd8dee9);
    t.terminal_bg      = Hex(0x2e3440);
    t.terminal_fg      = Hex(0xd8dee9);
    t.search_highlight = Hex(0xebcb8b, 0.35f);

    t.syntax_keyword      = Hex(0x81a1c1);
    t.syntax_type          = Hex(0x8fbcbb);
    t.syntax_string        = Hex(0xa3be8c);
    t.syntax_character     = Hex(0xa3be8c);
    t.syntax_number        = Hex(0xb48ead);
    t.syntax_comment       = Hex(0x616e88);
    t.syntax_preprocessor  = Hex(0xbf616a);
    t.syntax_operator      = Hex(0x81a1c1);
    t.syntax_punctuation   = Hex(0xd8dee9);
    t.syntax_function      = Hex(0x88c0d0);
    t.syntax_identifier    = Hex(0xd8dee9);
    t.syntax_namespace     = Hex(0x8fbcbb);
    t.syntax_macro         = Hex(0x88c0d0);
    t.syntax_attribute     = Hex(0xd08770);
    t.syntax_tag           = Hex(0x81a1c1);
    t.syntax_tag_bracket   = Hex(0xd8dee9);
    t.syntax_property      = Hex(0x88c0d0);
    t.syntax_value         = Hex(0xa3be8c);
    t.syntax_lifetime      = Hex(0xbf616a);
    t.syntax_escape        = Hex(0xd08770);

    return t;
}

// ── ThemeManager ──────────────────────────────────────────────────────────

ThemeManager::ThemeManager() {
    // Built-in themes
    themes_.push_back(MakeVSCodeDark2026()); // Default!
    themes_.push_back(MakeCatppuccinMocha());
    themes_.push_back(MakeOneDark());
    themes_.push_back(MakeNord());

    std::string exe_dir = platform::GetExecutableDir();
    search_dirs_ = {
        exe_dir + "/themes",
        exe_dir + "/../themes",
        "themes"
    };

    ReloadThemes();
}

bool ThemeManager::LoadThemeFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    // Start with default VS Code Dark base so missing properties have sane defaults
    Theme t = MakeVSCodeDark2026();
    std::string default_name = fs::path(filepath).stem().string();
    t.name = default_name;

    std::string line;
    bool in_block_comment = false;

    while (std::getline(file, line)) {
        // Strip block comments /* ... */
        if (in_block_comment) {
            auto end_pos = line.find("*/");
            if (end_pos != std::string::npos) {
                line = line.substr(end_pos + 2);
                in_block_comment = false;
            } else {
                continue;
            }
        }
        auto block_start = line.find("/*");
        if (block_start != std::string::npos) {
            auto block_end = line.find("*/", block_start + 2);
            if (block_end != std::string::npos) {
                line = line.substr(0, block_start) + line.substr(block_end + 2);
            } else {
                line = line.substr(0, block_start);
                in_block_comment = true;
            }
        }

        // Strip single line comments // ...
        auto line_comment = line.find("//");
        if (line_comment != std::string::npos) {
            line = line.substr(0, line_comment);
        }

        // Find delimiter ':' or '='
        auto delim = line.find(':');
        if (delim == std::string::npos) delim = line.find('=');
        if (delim == std::string::npos) continue;

        std::string key = line.substr(0, delim);
        std::string val = line.substr(delim + 1);

        // Trim key
        while (!key.empty() && (std::isspace(static_cast<unsigned char>(key.front())) || key.front() == '{')) key.erase(key.begin());
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
        std::ranges::transform(key, key.begin(), ::tolower);

        // Trim val
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(val.begin());
        while (!val.empty() && (std::isspace(static_cast<unsigned char>(val.back())) || val.back() == ';' || val.back() == '}')) val.pop_back();

        if (key.empty() || val.empty()) continue;

        if (key == "name" || key == "theme-name" || key == "title") {
            // Remove quotes if present
            std::string name_str = val;
            while (!name_str.empty() && (name_str.front() == '"' || name_str.front() == '\'')) name_str.erase(name_str.begin());
            while (!name_str.empty() && (name_str.back() == '"' || name_str.back() == '\'')) name_str.pop_back();
            if (!name_str.empty()) t.name = name_str;
            continue;
        }

        auto color_opt = ParseCSSColor(val);
        if (!color_opt.has_value()) continue;
        ImVec4 c = *color_opt;

        if (key == "background" || key == "bg" || key == "editor-bg") t.background = c;
        else if (key == "foreground" || key == "fg" || key == "color" || key == "text") t.foreground = c;
        else if (key == "gutter-bg" || key == "gutter-background" || key == "line-numbers-bg") t.gutter_bg = c;
        else if (key == "gutter-fg" || key == "gutter-foreground" || key == "line-numbers-fg") t.gutter_fg = c;
        else if (key == "active-line" || key == "current-line") t.active_line = c;
        else if (key == "cursor" || key == "cursor-color" || key == "caret") t.cursor_color = c;
        else if (key == "selection" || key == "selection-bg") t.selection = c;
        else if (key == "sidebar-bg" || key == "sidebar-background" || key == "explorer-bg") t.sidebar_bg = c;
        else if (key == "tab-bg" || key == "tab-background") t.tab_bg = c;
        else if (key == "tab-active-bg" || key == "tab-active-background") t.tab_active_bg = c;
        else if (key == "tab-text" || key == "tab-color") t.tab_text = c;
        else if (key == "tab-active-text" || key == "tab-active-color") t.tab_active_text = c;
        else if (key == "statusbar-bg" || key == "statusbar-background") t.statusbar_bg = c;
        else if (key == "statusbar-fg" || key == "statusbar-color") t.statusbar_fg = c;
        else if (key == "terminal-bg" || key == "terminal-background") t.terminal_bg = c;
        else if (key == "terminal-fg" || key == "terminal-color") t.terminal_fg = c;
        else if (key == "search-highlight") t.search_highlight = c;
        else if (key == "syntax-keyword" || key == "keyword") t.syntax_keyword = c;
        else if (key == "syntax-type" || key == "type") t.syntax_type = c;
        else if (key == "syntax-string" || key == "string") t.syntax_string = c;
        else if (key == "syntax-character" || key == "character" || key == "char") t.syntax_character = c;
        else if (key == "syntax-number" || key == "number") t.syntax_number = c;
        else if (key == "syntax-comment" || key == "comment") t.syntax_comment = c;
        else if (key == "syntax-preprocessor" || key == "preprocessor") t.syntax_preprocessor = c;
        else if (key == "syntax-operator" || key == "operator") t.syntax_operator = c;
        else if (key == "syntax-punctuation" || key == "punctuation") t.syntax_punctuation = c;
        else if (key == "syntax-function" || key == "function" || key == "fn") t.syntax_function = c;
        else if (key == "syntax-identifier" || key == "identifier") t.syntax_identifier = c;
        else if (key == "syntax-namespace" || key == "namespace") t.syntax_namespace = c;
        else if (key == "syntax-macro" || key == "macro") t.syntax_macro = c;
        else if (key == "syntax-attribute" || key == "attribute") t.syntax_attribute = c;
        else if (key == "syntax-tag" || key == "tag") t.syntax_tag = c;
        else if (key == "syntax-tag-bracket" || key == "tag-bracket") t.syntax_tag_bracket = c;
        else if (key == "syntax-property" || key == "property") t.syntax_property = c;
        else if (key == "syntax-value" || key == "value") t.syntax_value = c;
        else if (key == "syntax-lifetime" || key == "lifetime") t.syntax_lifetime = c;
        else if (key == "syntax-escape" || key == "escape") t.syntax_escape = c;
    }

    // Replace if theme already exists, else push
    for (size_t i = 0; i < themes_.size(); ++i) {
        if (themes_[i].name == t.name) {
            themes_[i] = t;
            return true;
        }
    }
    themes_.push_back(t);
    return true;
}

void ThemeManager::LoadThemesFromDirectory(const std::string& dir_path) {
    std::error_code ec;
    if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);
        if (ext == ".css" || ext == ".lucetheme") {
            LoadThemeFromFile(entry.path().string());
        }
    }
}

void ThemeManager::ReloadThemes() {
    std::string current_name = Active().name;

    // Preserve the built-in themes (first 4)
    if (themes_.size() > 4) {
        themes_.resize(4);
    }

    for (const auto& dir : search_dirs_) {
        LoadThemesFromDirectory(dir);
    }

    // Try to keep active theme
    SetTheme(current_name);
}

/// Apply the active theme's colours to ImGui's global style.
void ThemeManager::ApplyToImGui() const {
    const Theme& t = Active();
    ImGuiStyle& s  = ImGui::GetStyle();

    s.WindowRounding    = 0.0f;
    s.FrameRounding     = 2.0f;
    s.GrabRounding      = 2.0f;
    s.TabRounding       = 0.0f;
    s.ScrollbarRounding = 2.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.TabBorderSize     = 0.0f;
    s.WindowPadding     = ImVec2(8, 8);
    s.FramePadding      = ImVec2(6, 4);
    s.ItemSpacing       = ImVec2(6, 4);
    s.ScrollbarSize     = 10.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None; // Eliminate all triangle collapse / menu buttons globally!

    ImVec4* c = s.Colors;

    // Derive every UI colour from the active theme so custom themes actually
    // change the whole interface, not just the editor background.  Frames and
    // borders are computed by blending the background toward the foreground,
    // which works for both dark and light themes.  Interactive highlights use
    // the theme's cursor colour as an accent and its selection colour for
    // hovered/active headers.
    const ImVec4& bg     = t.background;
    const ImVec4& fg     = t.foreground;
    const ImVec4  accent = t.cursor_color;

    c[ImGuiCol_WindowBg]             = bg;
    c[ImGuiCol_ChildBg]              = bg;
    c[ImGuiCol_PopupBg]              = t.sidebar_bg;
    c[ImGuiCol_Border]               = Mix(bg, fg, 0.16f);
    c[ImGuiCol_FrameBg]              = Mix(bg, fg, 0.08f);
    c[ImGuiCol_FrameBgHovered]       = Mix(bg, fg, 0.16f);
    c[ImGuiCol_FrameBgActive]        = Mix(bg, fg, 0.24f);
    c[ImGuiCol_TitleBg]              = t.sidebar_bg;
    c[ImGuiCol_TitleBgActive]        = t.sidebar_bg;
    c[ImGuiCol_TitleBgCollapsed]     = t.sidebar_bg;
    c[ImGuiCol_MenuBarBg]            = t.sidebar_bg;
    c[ImGuiCol_ScrollbarBg]          = WithAlpha(bg, 0.0f);
    c[ImGuiCol_ScrollbarGrab]        = WithAlpha(Mix(bg, fg, 0.25f), 0.6f);
    c[ImGuiCol_ScrollbarGrabHovered] = WithAlpha(Mix(bg, fg, 0.35f), 0.8f);
    c[ImGuiCol_ScrollbarGrabActive]  = Mix(bg, fg, 0.45f);
    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accent;
    c[ImGuiCol_Button]               = Mix(bg, fg, 0.10f);
    c[ImGuiCol_ButtonHovered]        = Mix(bg, fg, 0.20f);
    c[ImGuiCol_ButtonActive]         = accent;
    c[ImGuiCol_Header]               = WithAlpha(t.selection, 0.55f);
    c[ImGuiCol_HeaderHovered]        = WithAlpha(t.selection, 0.80f);
    c[ImGuiCol_HeaderActive]         = t.selection;
    c[ImGuiCol_Separator]            = Mix(bg, fg, 0.16f);
    c[ImGuiCol_SeparatorHovered]     = accent;
    c[ImGuiCol_SeparatorActive]      = accent;
    c[ImGuiCol_Tab]                  = t.tab_bg;
    c[ImGuiCol_TabHovered]           = t.tab_active_bg;
    c[ImGuiCol_TabSelected]          = t.tab_active_bg;
    c[ImGuiCol_TabDimmed]            = t.tab_bg;
    c[ImGuiCol_TabDimmedSelected]    = t.tab_active_bg;
    c[ImGuiCol_Text]                 = t.foreground;
    c[ImGuiCol_TextDisabled]         = Mix(fg, bg, 0.5f);
    c[ImGuiCol_DockingPreview]       = WithAlpha(accent, 0.35f);
    c[ImGuiCol_DockingEmptyBg]       = bg;
    c[ImGuiCol_NavHighlight]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void ThemeManager::SetTheme(const std::string& name) {
    for (int i = 0; i < static_cast<int>(themes_.size()); ++i) {
        if (themes_[i].name == name) {
            active_index_ = i;
            return;
        }
    }
}

void ThemeManager::CycleTheme() {
    active_index_ = (active_index_ + 1) % static_cast<int>(themes_.size());
}

std::vector<std::string> ThemeManager::GetThemeNames() const {
    std::vector<std::string> names;
    for (auto& t : themes_) names.push_back(t.name);
    return names;
}

}  // namespace luce
