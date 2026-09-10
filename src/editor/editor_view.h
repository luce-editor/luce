#pragma once
// ============================================================================
// EditorView — The core code editing widget rendered via ImGui.
//
// Responsibilities:
//   • Render text with per-token syntax colours using ImDrawList.
//   • Line numbers, active line highlight, cursor, selection.
//   • Keyboard & mouse input handling (navigation, editing, clipboard).
//   • Find / Replace overlay (Ctrl+F / Ctrl+H).
//   • Multi-cursor support (Ctrl+Click, Ctrl+D).
//   • Virtual scrolling (only visible lines are drawn).
// ============================================================================

#include "editor/cursor.h"
#include "editor/text_buffer.h"
#include "editor/symbol_index.h"
#include "editor/git_manager.h"
#include "syntax/syntax_highlighter.h"
#include "ui/theme.h"

#include "imgui.h"
#include <filesystem>
#include <optional>
#include <string>

namespace luce {

class EditorView {
public:
    EditorView();

    /// Bind the view to a buffer and highlighter.
    void SetBuffer(TextBuffer* buffer);
    void SetHighlighter(SyntaxHighlighter* highlighter);
    void SetTheme(const Theme* theme);
    /// Set the path of the file currently loaded — used for include path autocompletion and git diff.
    void SetFilePath(const std::string& path) { current_file_path_ = path; RefreshGitDiff(); }

    /// Main render call — draws the entire editor widget.
    /// `id` should be unique per ImGui window (e.g. "editor_0").
    void Render(const char* id);

    // ── Accessors ─────────────────────────────────────────────────────────

    CursorSet&       GetCursors()       { return cursors_; }
    const CursorSet& GetCursors() const { return cursors_; }
    TextBuffer*      GetBuffer()        { return buffer_; }
    bool             IsFocused() const  { return focused_; }

    // ── Public actions (can be called from Command Palette / keybindings) ─

    void Undo();
    void Redo();
    void Cut();
    void Copy();
    void Paste();
    void OpenFind();
    void OpenReplace();
    void CloseFind();
    void GoToLine(int line);
    void GoToPosition(int line, int column);
    void Focus() { needs_focus_ = true; }
    void EnsureCursorVisible();
    void SelectAll();
    void ToggleComment();

    // ── Symbol navigation ────────────────────────────────────────────────
    void SetSymbolIndex(class SymbolIndex* index) { symbol_index_ = index; }
    void SetOnGoToDefinition(std::function<void(const std::string& path, int line)> cb) {
        on_goto_definition_ = std::move(cb);
    }
    /// Jump to the definition of the symbol under mouse or cursor (Ctrl+Click, Ctrl+Enter, F12).
    bool GoToDefinition();
    bool HasHoveredSymbol() const { return hovered_symbol_.has_value(); }
    const std::optional<SymbolInfo>& GetHoveredSymbol() const { return hovered_symbol_; }

    // ── Settings ──────────────────────────────────────────────────────────

    int  tab_size      = 4;
    bool use_spaces    = true;
    bool show_minimap  = true;

private:
    // ── Rendering helpers ─────────────────────────────────────────────────
    void RenderMinimap(ImDrawList* dl, ImVec2 origin, float line_height,
                       float char_width, int total_lines, int first_line,
                       int last_line, float gutter_width);
    void RenderSymbolHoverAndNavigation(ImDrawList* dl, ImVec2 origin,
                                        float line_height, float char_width,
                                        float gutter_width);
    void RenderGutter(ImDrawList* dl, ImVec2 origin, float line_height,
                      int first_line, int last_line, float gutter_width);
    void RenderLines(ImDrawList* dl, ImVec2 origin, float line_height,
                     float char_width, int first_line, int last_line,
                     float gutter_width);
    void RenderDiagnostics(ImDrawList* dl, ImVec2 origin, float line_height,
                           float char_width, int first_line, int last_line,
                           float gutter_width);
    void RenderSelections(ImDrawList* dl, ImVec2 origin, float line_height,
                          float char_width, int first_line, int last_line,
                          float gutter_width);
    void RenderCursors(ImDrawList* dl, ImVec2 origin, float line_height,
                       float char_width, float gutter_width);
    void RenderActiveLineHighlight(ImDrawList* dl, ImVec2 origin,
                                    float line_height, float gutter_width,
                                    float window_width);
    void RenderFindBar();

    // ── Input handling ────────────────────────────────────────────────────
    void HandleKeyboardInput();
    void HandleMouseInput(ImVec2 origin, float line_height, float char_width,
                          float gutter_width);
    void HandleTextInput();

    // ── Cursor movement ───────────────────────────────────────────────────
    void MoveCursorUp(bool extend);
    void MoveCursorDown(bool extend);
    void MoveCursorLeft(bool extend);
    void MoveCursorRight(bool extend);
    void MoveCursorWordLeft(bool extend);
    void MoveCursorWordRight(bool extend);
    void MoveCursorHome(bool extend);
    void MoveCursorEnd(bool extend);
    void MoveCursorPageUp(bool extend);
    void MoveCursorPageDown(bool extend);

    // ── Editing ───────────────────────────────────────────────────────────
    void InsertCharAtCursors(const std::string& text);
    void DeleteAtCursors(bool forward);
    void DeleteWordAtCursors(bool forward);
    void InsertNewLine();
    void HandleTab(bool shift);

    // ── Selection ─────────────────────────────────────────────────────────
    void DeleteSelection(Cursor& c);
    std::string GetSelectionText(const Cursor& c) const;
    void SelectNextOccurrence();

    // ── Find ──────────────────────────────────────────────────────────────
    void FindNext();
    void FindPrev();
    void ReplaceNext();
    void ReplaceAll();

    // ── Utility ───────────────────────────────────────────────────────────
    float CalculateGutterWidth() const;
    TextPosition ScreenToTextPosition(ImVec2 origin, ImVec2 mouse,
                                       float line_height, float char_width,
                                       float gutter_width) const;
    int GetWordBoundaryLeft(int line, int col) const;
    int GetWordBoundaryRight(int line, int col) const;
    std::string GetAutoIndent(int line) const;

    // ── State ─────────────────────────────────────────────────────────────
    TextBuffer*          buffer_      = nullptr;
    SyntaxHighlighter*   highlighter_ = nullptr;
    const Theme*         theme_       = nullptr;
    CursorSet            cursors_;
    bool                 focused_     = false;
    float                scroll_x_    = 0.0f;
    float                scroll_y_    = 0.0f;
    double               cursor_blink_time_ = 0.0;
    int                  visible_line_count_ = 0;
    /// Set to true whenever the cursor moves; cleared after EnsureCursorVisible().
    bool                 needs_scroll_to_cursor_ = false;
    /// Set to true to request ImGui window focus on next render frame.
    bool                 needs_focus_ = false;

    // Find/Replace state
    bool        find_open_    = false;
    bool        replace_open_ = false;
    char        find_buf_[256]    = {};
    char        replace_buf_[256] = {};
    int         find_line_ = -1;
    int         find_col_  = -1;

    // Autocomplete state
    void RenderAutocomplete(ImVec2 origin, float line_height, float char_width, float gutter_width);
    void UpdateAutocomplete();
    void ApplyAutocomplete();

    /// Current file path — used to resolve relative includes.
    std::string              current_file_path_;

    bool                     ac_open_        = false;
    bool                     ac_include_mode_ = false;  ///< True when completing an #include path.
    std::string              ac_prefix_;
    std::vector<std::string> ac_suggestions_;
    int                      ac_selected_    = 0;

    // Symbol navigation & minimap
    class SymbolIndex*                                      symbol_index_ = nullptr;
    std::function<void(const std::string& path, int line)> on_goto_definition_;
    std::function<std::vector<std::string>(const std::string&, const std::string&, int, int)> completion_provider_;
    bool                                                   minimap_dragging_ = false;

public:
    void SetCompletionProvider(std::function<std::vector<std::string>(const std::string&, const std::string&, int, int)> cb) {
        completion_provider_ = std::move(cb);
    }
    void RefreshGitDiff();

private:
    struct GitFileDiffMarks                                git_diff_marks_;
    float                                                  git_diff_timer_ = 0.0f;

    // Cached render geometry for symbol resolution and keyboard shortcuts
    ImVec2                                                 last_origin_ = ImVec2(0, 0);
    float                                                  last_line_height_ = 0.0f;
    float                                                  last_char_width_ = 0.0f;
    float                                                  last_gutter_width_ = 0.0f;
    std::optional<SymbolInfo>                              hovered_symbol_;
};

}  // namespace luce
