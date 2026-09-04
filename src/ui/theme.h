#pragma once
// ============================================================================
// Theme — Colour definitions for the editor UI and syntax highlighting.
//
// Provides a mapping from TokenType → ImVec4 colour, plus colours for
// UI elements (background, gutter, cursor, selection, etc.).
// Ships with several built-in themes; the active one can be switched
// at runtime.
// ============================================================================

#include "imgui.h"
#include "syntax/lexer.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace luce {

struct Theme {
    std::string name;

    // ── UI colours ────────────────────────────────────────────────────────
    ImVec4 background;
    ImVec4 foreground;
    ImVec4 gutter_bg;
    ImVec4 gutter_fg;
    ImVec4 active_line;
    ImVec4 cursor_color;
    ImVec4 selection;
    ImVec4 sidebar_bg;
    ImVec4 tab_bg;
    ImVec4 tab_active_bg;
    ImVec4 tab_text;
    ImVec4 tab_active_text;
    ImVec4 statusbar_bg;
    ImVec4 statusbar_fg;
    ImVec4 terminal_bg;
    ImVec4 terminal_fg;
    ImVec4 search_highlight;

    // ── Syntax colours ────────────────────────────────────────────────────
    ImVec4 syntax_keyword;
    ImVec4 syntax_type;
    ImVec4 syntax_string;
    ImVec4 syntax_character;
    ImVec4 syntax_number;
    ImVec4 syntax_comment;
    ImVec4 syntax_preprocessor;
    ImVec4 syntax_operator;
    ImVec4 syntax_punctuation;
    ImVec4 syntax_function;
    ImVec4 syntax_identifier;
    ImVec4 syntax_namespace;
    ImVec4 syntax_macro;
    ImVec4 syntax_attribute;
    ImVec4 syntax_tag;
    ImVec4 syntax_tag_bracket;
    ImVec4 syntax_property;
    ImVec4 syntax_value;
    ImVec4 syntax_lifetime;
    ImVec4 syntax_escape;

    /// Map a TokenType to its colour in this theme.
    ImVec4 GetTokenColor(TokenType tt) const;
};

/// Global theme manager.  Owns a set of built-in themes and exposes the
/// currently active one.
class ThemeManager {
public:
    ThemeManager();

    /// Apply the theme's colours to ImGui's style (window rounding, colours, etc.).
    void ApplyToImGui() const;

    const Theme& Active() const { return themes_[active_index_]; }
    void SetTheme(const std::string& name);
    void CycleTheme();

    std::vector<std::string> GetThemeNames() const;

    /// Load custom themes from a CSS / .lucetheme file.
    bool LoadThemeFromFile(const std::string& filepath);

    /// Scan a directory for .css and .lucetheme files and load all custom themes.
    void LoadThemesFromDirectory(const std::string& dir_path);

    /// Reload custom themes from standard search paths.
    void ReloadThemes();

private:
    std::vector<Theme> themes_;
    int active_index_ = 0;
    std::vector<std::string> search_dirs_;
};

}  // namespace luce
