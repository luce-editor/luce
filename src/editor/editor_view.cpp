// ============================================================================
// EditorView — Implementation.
//
// The editor is rendered as a single ImGui child window with custom drawing.
// Text is drawn token-by-token via ImDrawList for syntax colouring.
// Only visible lines are processed (virtual scrolling).
// ============================================================================

#include "editor_view.h"
#include "diagnostic.h"

#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <regex>
#include <set>
#include <sstream>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#endif

namespace luce {

EditorView::EditorView() = default;

void EditorView::SetBuffer(TextBuffer* b) { buffer_ = b; }
void EditorView::SetHighlighter(SyntaxHighlighter* h) { highlighter_ = h; }
void EditorView::SetTheme(const Theme* t) { theme_ = t; }

// ── Main render ───────────────────────────────────────────────────────────

void EditorView::Render(const char* id) {
    if (!buffer_ || !theme_) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme_->background);
    ImGui::BeginChild(id, ImVec2(0, 0), false,
        ImGuiWindowFlags_HorizontalScrollbar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNavInputs);

    focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    // ImGui's core navigation drops window focus when Escape is pressed.
    // If autocomplete is open, we intercept this, close it, and forcefully restore focus.
    if (ac_open_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ac_open_ = false;
        focused_ = true;
        ImGui::SetWindowFocus();
    }
    ImDrawList* dl        = ImGui::GetWindowDrawList();
    float line_height     = ImGui::GetTextLineHeightWithSpacing();
    float char_width      = ImGui::CalcTextSize("A").x;
    float gutter_width    = CalculateGutterWidth();
    float window_height   = ImGui::GetWindowHeight();
    float window_width    = ImGui::GetWindowWidth();

    int total_lines       = buffer_->GetLineCount();
    int first_line        = static_cast<int>(ImGui::GetScrollY() / line_height);
    int last_line         = std::min(total_lines,
                                     first_line + static_cast<int>(window_height / line_height) + 2);
    visible_line_count_   = last_line - first_line;

    // We position the ImGui cursor at the first visible line's Y position,
    // then use the draw list to render everything manually.
    ImVec2 origin = ImGui::GetCursorScreenPos();
    origin.y -= ImGui::GetScrollY() - first_line * line_height;
    origin.x -= ImGui::GetScrollX();

    // Active line highlight (full-width background bar).
    RenderActiveLineHighlight(dl, origin, line_height, gutter_width, window_width + ImGui::GetScrollX());

    // Selection rectangles.
    RenderSelections(dl, origin, line_height, char_width, first_line, last_line, gutter_width);

    // Gutter (line numbers).
    RenderGutter(dl, origin, line_height, first_line, last_line, gutter_width);

    // Source code text with syntax colouring.
    RenderLines(dl, origin, line_height, char_width, first_line, last_line, gutter_width);

    // Diagnostics (red/yellow squiggly underlines).
    RenderDiagnostics(dl, origin, line_height, char_width, first_line, last_line, gutter_width);

    // Blinking cursors.
    RenderCursors(dl, origin, line_height, char_width, gutter_width);

    // Set the dummy size for scrolling.
    // The canvas x-range covers the gutter + text content + a right margin.
    // scroll_x is the canvas offset; gutter rendering adds scroll_x back to stay pinned.
    float max_line_len = 0;
    for (int i = 0; i < total_lines; ++i) {
        float w = static_cast<float>(buffer_->GetLine(i).size()) * char_width;
        if (w > max_line_len) max_line_len = w;
    }
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Dummy(ImVec2(gutter_width + max_line_len + char_width * 20,
                         (total_lines + 8) * line_height));

    // Handle mouse wheel scrolling explicitly when hovered
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
        float scroll_delta = -ImGui::GetIO().MouseWheel * line_height * 3.0f;
        ImGui::SetScrollY(std::clamp(ImGui::GetScrollY() + scroll_delta, 0.0f, ImGui::GetScrollMaxY()));
    }

    // Set mouse cursor to I-beam when hovering over text area
    if (ImGui::IsWindowHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    }

    // Input handling — always active when the editor has focus.
    // Note: HandleTextInput() already ignores characters while WantTextInput is
    // captured by another widget (e.g. find bar). Ctrl-shortcuts (Ctrl+Z, etc.)
    // must never be gated behind WantTextInput or they silently stop working.
    if (focused_) {
        HandleMouseInput(origin, line_height, char_width, gutter_width);
        HandleKeyboardInput();
        HandleTextInput();
    }

    // Only scroll to the cursor when something actually moved it.
    if (needs_scroll_to_cursor_) {
        EnsureCursorVisible();
        needs_scroll_to_cursor_ = false;
    }

    // Render autocomplete suggestions popup right above/below cursor
    if (ac_open_ && focused_) {
        RenderAutocomplete(origin, line_height, char_width, gutter_width);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Find/Replace bar (rendered outside the child window).
    if (find_open_) RenderFindBar();
}

// ── Gutter ────────────────────────────────────────────────────────────────

void EditorView::RenderGutter(ImDrawList* dl, ImVec2 origin, float lh,
                               int first, int last, float gw) {
    float scroll_x = ImGui::GetScrollX();

    // Gutter background — a solid strip on the left.
    dl->AddRectFilled(
        ImVec2(origin.x + scroll_x, origin.y + first * lh),
        ImVec2(origin.x + scroll_x + gw - 4.0f, origin.y + last * lh),
        ImGui::ColorConvertFloat4ToU32(theme_->gutter_bg));

    ImU32 fg   = ImGui::ColorConvertFloat4ToU32(theme_->gutter_fg);
    ImU32 active_fg = ImGui::ColorConvertFloat4ToU32(theme_->foreground);
    int cursor_line = cursors_.Primary().position.line;

    for (int i = first; i < last; ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", i + 1);
        float text_width = ImGui::CalcTextSize(buf).x;
        float x = origin.x + scroll_x + gw - text_width - 12.0f;
        float y = origin.y + i * lh;
        dl->AddText(ImVec2(x, y), (i == cursor_line) ? active_fg : fg, buf);
    }
}

// ── Lines (syntax-coloured text) ──────────────────────────────────────────

void EditorView::RenderLines(ImDrawList* dl, ImVec2 origin, float lh,
                              float cw, int first, int last, float gw) {
    ImU32 default_color = ImGui::ColorConvertFloat4ToU32(theme_->foreground);

    for (int i = first; i < last; ++i) {
        const std::string& line = buffer_->GetLine(i);
        float base_x = origin.x + gw;
        float y      = origin.y + i * lh;

        if (highlighter_) {
            const auto& tokens = highlighter_->GetTokensForLine(i, line);
            if (tokens.empty()) {
                // No tokens — draw the whole line as plain text.
                if (!line.empty())
                    dl->AddText(ImVec2(base_x, y), default_color, line.c_str());
            } else {
                int prev_end = 0;
                for (auto& tok : tokens) {
                    // Draw any gap before this token as default text.
                    if (tok.start > prev_end) {
                        std::string gap = line.substr(prev_end, tok.start - prev_end);
                        dl->AddText(ImVec2(base_x + prev_end * cw, y),
                                    default_color, gap.c_str());
                    }
                    ImU32 color = ImGui::ColorConvertFloat4ToU32(
                                      theme_->GetTokenColor(tok.type));
                    int end_col = std::min(tok.start + tok.length,
                                           static_cast<int>(line.size()));
                    std::string text = line.substr(tok.start, end_col - tok.start);
                    dl->AddText(ImVec2(base_x + tok.start * cw, y), color,
                                text.c_str());
                    prev_end = end_col;
                }
                // Draw any trailing text after the last token.
                if (prev_end < static_cast<int>(line.size())) {
                    std::string tail = line.substr(prev_end);
                    dl->AddText(ImVec2(base_x + prev_end * cw, y),
                                default_color, tail.c_str());
                }
            }
        } else {
            if (!line.empty())
                dl->AddText(ImVec2(base_x, y), default_color, line.c_str());
        }
    }
}

// ── Diagnostics (squiggles & tooltips) ───────────────────────────────────

void EditorView::RenderDiagnostics(ImDrawList* dl, ImVec2 origin, float lh,
                                   float cw, int first, int last, float gw) {
    if (current_file_path_.empty()) return;

    std::string norm_path = current_file_path_;
    std::ranges::replace(norm_path, '\\', '/');

    auto diags = DiagnosticManager::Instance().GetDiagnosticsForFile(norm_path);
    if (diags.empty()) return;

    ImVec2 mouse_pos = ImGui::GetMousePos();
    std::string hovered_msg;

    for (const auto& diag : diags) {
        int line_idx = diag.line - 1; // 1-based to 0-based
        if (line_idx < first || line_idx >= last || line_idx >= buffer_->GetLineCount()) continue;

        const std::string& line_text = buffer_->GetLine(line_idx);
        int col_start = std::max(0, diag.column - 1);
        int col_end = col_start + 4; // default minimum span
        if (col_start < static_cast<int>(line_text.size())) {
            int peek = col_start;
            while (peek < static_cast<int>(line_text.size()) &&
                   line_text[peek] != ' ' && line_text[peek] != '\t' && line_text[peek] != ';') {
                peek++;
            }
            if (peek > col_start) col_end = peek;
        }

        float x1 = origin.x + gw + col_start * cw;
        float x2 = origin.x + gw + std::max(col_end, col_start + 1) * cw;
        float y  = origin.y + (line_idx + 1) * lh - 1.5f;

        ImU32 squiggle_col = (diag.severity == DiagnosticSeverity::Error) ?
                             IM_COL32(235, 75, 75, 230) : IM_COL32(235, 185, 45, 230);

        // Draw wavy squiggly line
        float step = 3.0f;
        float wave_h = 2.0f;
        bool up = true;
        for (float curr_x = x1; curr_x < x2; curr_x += step) {
            float next_x = std::min(curr_x + step, x2);
            float curr_y = up ? (y - wave_h) : y;
            float next_y = up ? y : (y - wave_h);
            dl->AddLine(ImVec2(curr_x, curr_y), ImVec2(next_x, next_y), squiggle_col, 1.2f);
            up = !up;
        }

        // Hover tooltip detection
        if (mouse_pos.x >= x1 && mouse_pos.x <= x2 && mouse_pos.y >= (y - lh) && mouse_pos.y <= y + 4.0f) {
            hovered_msg = (diag.severity == DiagnosticSeverity::Error ? "Error: " : "Warning: ") + diag.message;
        }
    }

    if (!hovered_msg.empty() && ImGui::IsWindowHovered()) {
        ImGui::SetTooltip("%s", hovered_msg.c_str());
    }
}

// ── Selection rectangles ──────────────────────────────────────────────────

void EditorView::RenderSelections(ImDrawList* dl, ImVec2 origin, float lh,
                                   float cw, int first, int last, float gw) {
    ImU32 sel_color = ImGui::ColorConvertFloat4ToU32(theme_->selection);

    for (auto& cursor : cursors_.cursors) {
        if (!cursor.HasSelection()) continue;

        TextPosition begin = cursor.SelectionBegin();
        TextPosition end   = cursor.SelectionEnd();

        for (int line = std::max(begin.line, first); line <= std::min(end.line, last - 1); ++line) {
            int start_col = (line == begin.line) ? begin.column : 0;
            int end_col   = (line == end.line)   ? end.column
                                                  : static_cast<int>(buffer_->GetLine(line).size());

            float x1 = origin.x + gw + start_col * cw;
            float x2 = origin.x + gw + end_col * cw;
            float y  = origin.y + line * lh;
            dl->AddRectFilled(ImVec2(x1, y), ImVec2(x2, y + lh), sel_color);
        }
    }
}

// ── Cursor rendering ─────────────────────────────────────────────────────

void EditorView::RenderCursors(ImDrawList* dl, ImVec2 origin, float lh,
                                float cw, float gw) {
    // Blink every 0.53 seconds (like VS Code).
    cursor_blink_time_ += ImGui::GetIO().DeltaTime;
    bool visible = fmod(cursor_blink_time_, 1.06) < 0.53;
    if (!focused_) visible = false;

    if (!visible) return;

    ImU32 color = ImGui::ColorConvertFloat4ToU32(theme_->cursor_color);

    for (auto& cursor : cursors_.cursors) {
        float x = origin.x + gw + cursor.position.column * cw;
        float y = origin.y + cursor.position.line * lh;
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 2.0f, y + lh), color);
    }
}

// ── Active line highlight ─────────────────────────────────────────────────

void EditorView::RenderActiveLineHighlight(ImDrawList* dl, ImVec2 origin,
                                            float lh, float gw, float ww) {
    ImU32 color = ImGui::ColorConvertFloat4ToU32(theme_->active_line);
    for (auto& cursor : cursors_.cursors) {
        float y = origin.y + cursor.position.line * lh;
        dl->AddRectFilled(ImVec2(origin.x, y),
                          ImVec2(origin.x + ww, y + lh), color);
    }
}

// ── Find bar ──────────────────────────────────────────────────────────────

void EditorView::RenderFindBar() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 360,
                                   ImGui::GetWindowPos().y + 4));
    ImGui::SetNextWindowSize(ImVec2(350, replace_open_ ? 72 : 36));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme_->sidebar_bg);
    ImGui::Begin("##find_bar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::SetNextItemWidth(220);
    if (ImGui::InputText("##find", find_buf_, sizeof(find_buf_),
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
        FindNext();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Next"))  FindNext();
    ImGui::SameLine();
    if (ImGui::SmallButton("Prev"))  FindPrev();
    ImGui::SameLine();
    if (ImGui::SmallButton("X"))     CloseFind();

    if (replace_open_) {
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("##replace", replace_buf_, sizeof(replace_buf_));
        ImGui::SameLine();
        if (ImGui::SmallButton("Repl"))    ReplaceNext();
        ImGui::SameLine();
        if (ImGui::SmallButton("All"))     ReplaceAll();
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

// ── Keyboard input ────────────────────────────────────────────────────────

/// Centralised keyboard shortcut handler.  Modifier keys are read from
/// ImGui's IO state; regular keys are checked with IsKeyPressed().
void EditorView::HandleKeyboardInput() {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl   = io.KeyCtrl;
    bool shift  = io.KeyShift;
    bool alt    = io.KeyAlt;

    // Autocomplete navigation
    if (ac_open_) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            ac_selected_ = (ac_selected_ - 1 + static_cast<int>(ac_suggestions_.size())) % static_cast<int>(ac_suggestions_.size());
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            ac_selected_ = (ac_selected_ + 1) % static_cast<int>(ac_suggestions_.size());
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            ApplyAutocomplete();
            return;
        }
        // Escape is now handled globally at the start of EditorView::Render()
        // to prevent ImGui from silently dropping window focus before we reach here.
    }

    // Navigation
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    MoveCursorUp(shift);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))  MoveCursorDown(shift);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        if (ctrl) MoveCursorWordLeft(shift); else MoveCursorLeft(shift);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        if (ctrl) MoveCursorWordRight(shift); else MoveCursorRight(shift);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home))     MoveCursorHome(shift);
    if (ImGui::IsKeyPressed(ImGuiKey_End))      MoveCursorEnd(shift);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))   MoveCursorPageUp(shift);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) MoveCursorPageDown(shift);

    // Editing
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        if (ctrl) DeleteWordAtCursors(false); else DeleteAtCursors(false);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (ctrl) DeleteWordAtCursors(true); else DeleteAtCursors(true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter))     InsertNewLine();
    if (ImGui::IsKeyPressed(ImGuiKey_Tab))       HandleTab(shift);

    // Clipboard
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) Copy();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) Cut();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) Paste();

    // Undo / Redo
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z)) Undo();
    if (ctrl && shift  && ImGui::IsKeyPressed(ImGuiKey_Z)) Redo();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y))           Redo();

    // Select all
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) SelectAll();

    // Find / Replace
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F)) OpenFind();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_H)) OpenReplace();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))    CloseFind();

    // Select next occurrence (Ctrl+D)
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) SelectNextOccurrence();

    // Toggle comment (Ctrl+/)
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Slash)) ToggleComment();

    // Reset blink on any input.
    cursor_blink_time_ = 0.0;
}

// ── Mouse input ───────────────────────────────────────────────────────────

void EditorView::HandleMouseInput(ImVec2 origin, float lh, float cw, float gw) {
    ImGuiIO& io = ImGui::GetIO();
    float scroll_x = ImGui::GetScrollX();
    float scroll_y = ImGui::GetScrollY();
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

    // Check if mouse is on the vertical scrollbar area (right 16px) or horizontal scrollbar (bottom 16px)
    bool on_v_scrollbar = (io.MousePos.x >= win_pos.x + win_size.x - 16.0f);
    bool on_h_scrollbar = (io.MousePos.y >= win_pos.y + win_size.y - 16.0f);

    if (on_v_scrollbar || on_h_scrollbar) {
        return; // Don't drag-select when user interacts with scrollbars
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        TextPosition pos = ScreenToTextPosition(origin, io.MousePos, lh, cw, gw);

        if (io.KeyCtrl) {
            // Ctrl+Click: add cursor.
            cursors_.AddCursor(pos);
        } else {
            // Regular click: single cursor.
            cursors_.ResetToSingle();
            cursors_.Primary().MoveTo(pos, io.KeyShift);
        }
        cursor_blink_time_ = 0.0;
        needs_scroll_to_cursor_ = true;
    }

    // Drag to select (only when dragging started inside text viewport).
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        ImVec2 click_pos = io.MouseClickedPos[0];
        bool clicked_on_scrollbar = (click_pos.x >= win_pos.x + win_size.x - 16.0f) ||
                                    (click_pos.y >= win_pos.y + win_size.y - 16.0f);
        if (!clicked_on_scrollbar) {
            TextPosition pos = ScreenToTextPosition(origin, io.MousePos, lh, cw, gw);
            cursors_.Primary().position = pos;
            cursor_blink_time_ = 0.0;
            // Do NOT set needs_scroll_to_cursor_ here — the user is dragging
            // and the viewport should follow the mouse naturally.
        }
    }

    // Double-click to select word.
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        TextPosition pos = ScreenToTextPosition(origin, io.MousePos, lh, cw, gw);
        cursors_.ResetToSingle();
        int left  = GetWordBoundaryLeft(pos.line, pos.column);
        int right = GetWordBoundaryRight(pos.line, pos.column);
        cursors_.Primary().selection_start = {pos.line, left};
        cursors_.Primary().position        = {pos.line, right};
        needs_scroll_to_cursor_ = true;
    }
}

// ── Text input (printable characters) ─────────────────────────────────────

void EditorView::HandleTextInput() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl || io.KeyAlt) return;  // Don't process Ctrl/Alt combos.

    for (int n = 0; n < io.InputQueueCharacters.Size; ++n) {
        ImWchar ch = io.InputQueueCharacters[n];
        if (ch == '\t' || ch == '\r' || ch == '\n') continue;
        if (ch < 32 && ch != '\t') continue;

        char buf[5] = {};
        // Convert ImWchar (UTF-16) to UTF-8.
        if (ch < 0x80) {
            buf[0] = static_cast<char>(ch);
        } else if (ch < 0x800) {
            buf[0] = static_cast<char>(0xC0 | (ch >> 6));
            buf[1] = static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            buf[0] = static_cast<char>(0xE0 | (ch >> 12));
            buf[1] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            buf[2] = static_cast<char>(0x80 | (ch & 0x3F));
        }
        InsertCharAtCursors(buf);
        
        // Auto-close brackets and quotes
        if (ch == '(' || ch == '[' || ch == '{' || ch == '"' || ch == '\'') {
            const char* close_ch = (ch == '(') ? ")" : 
                                   (ch == '[') ? "]" : 
                                   (ch == '{') ? "}" : 
                                   (ch == '"') ? "\"" : "'";
            InsertCharAtCursors(close_ch);
            MoveCursorLeft(false);
        } else if (ch == '<') {
            const auto& c = cursors_.Primary();
            const auto& line = buffer_->GetLine(c.position.line);
            std::string trimmed = line;
            size_t first = trimmed.find_first_not_of(" \t");
            if (first != std::string::npos) trimmed = trimmed.substr(first);
            if (trimmed.starts_with("#include")) {
                InsertCharAtCursors(">");
                MoveCursorLeft(false);
            }
        }
        
        UpdateAutocomplete();
    }
}

// ── Cursor movement ───────────────────────────────────────────────────────

void EditorView::MoveCursorUp(bool ext) {
    for (auto& c : cursors_.cursors) {
        if (c.position.line > 0) {
            int new_col = std::min(c.position.column,
                                   static_cast<int>(buffer_->GetLine(c.position.line - 1).size()));
            c.MoveTo({c.position.line - 1, new_col}, ext);
        }
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorDown(bool ext) {
    for (auto& c : cursors_.cursors) {
        if (c.position.line < buffer_->GetLineCount() - 1) {
            int new_col = std::min(c.position.column,
                                   static_cast<int>(buffer_->GetLine(c.position.line + 1).size()));
            c.MoveTo({c.position.line + 1, new_col}, ext);
        }
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorLeft(bool ext) {
    for (auto& c : cursors_.cursors) {
        if (!ext && c.HasSelection()) {
            c.MoveTo(c.SelectionBegin());
        } else if (c.position.column > 0) {
            c.MoveTo({c.position.line, c.position.column - 1}, ext);
        } else if (c.position.line > 0) {
            int end = static_cast<int>(buffer_->GetLine(c.position.line - 1).size());
            c.MoveTo({c.position.line - 1, end}, ext);
        }
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorRight(bool ext) {
    for (auto& c : cursors_.cursors) {
        int line_len = static_cast<int>(buffer_->GetLine(c.position.line).size());
        if (!ext && c.HasSelection()) {
            c.MoveTo(c.SelectionEnd());
        } else if (c.position.column < line_len) {
            c.MoveTo({c.position.line, c.position.column + 1}, ext);
        } else if (c.position.line < buffer_->GetLineCount() - 1) {
            c.MoveTo({c.position.line + 1, 0}, ext);
        }
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorWordLeft(bool ext) {
    for (auto& c : cursors_.cursors) {
        int col = GetWordBoundaryLeft(c.position.line, c.position.column);
        if (col == c.position.column && c.position.line > 0) {
            int end = static_cast<int>(buffer_->GetLine(c.position.line - 1).size());
            c.MoveTo({c.position.line - 1, end}, ext);
        } else {
            c.MoveTo({c.position.line, col}, ext);
        }
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorWordRight(bool ext) {
    for (auto& c : cursors_.cursors) {
        int line_len = static_cast<int>(buffer_->GetLine(c.position.line).size());
        int col = GetWordBoundaryRight(c.position.line, c.position.column);
        if (col == c.position.column && c.position.line < buffer_->GetLineCount() - 1) {
            c.MoveTo({c.position.line + 1, 0}, ext);
        } else {
            c.MoveTo({c.position.line, col}, ext);
        }
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorHome(bool ext) {
    for (auto& c : cursors_.cursors) {
        // Smart home: go to first non-whitespace character, or column 0.
        const auto& line = buffer_->GetLine(c.position.line);
        int first_non_ws = 0;
        while (first_non_ws < static_cast<int>(line.size()) &&
               std::isspace(static_cast<unsigned char>(line[first_non_ws])))
            ++first_non_ws;

        int target = (c.position.column == first_non_ws) ? 0 : first_non_ws;
        c.MoveTo({c.position.line, target}, ext);
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorEnd(bool ext) {
    for (auto& c : cursors_.cursors) {
        int end = static_cast<int>(buffer_->GetLine(c.position.line).size());
        c.MoveTo({c.position.line, end}, ext);
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorPageUp(bool ext) {
    for (auto& c : cursors_.cursors) {
        int new_line = std::max(0, c.position.line - visible_line_count_);
        int new_col  = std::min(c.position.column,
                                static_cast<int>(buffer_->GetLine(new_line).size()));
        c.MoveTo({new_line, new_col}, ext);
    }
    needs_scroll_to_cursor_ = true;
}

void EditorView::MoveCursorPageDown(bool ext) {
    for (auto& c : cursors_.cursors) {
        int new_line = std::min(buffer_->GetLineCount() - 1,
                                c.position.line + visible_line_count_);
        int new_col  = std::min(c.position.column,
                                static_cast<int>(buffer_->GetLine(new_line).size()));
        c.MoveTo({new_line, new_col}, ext);
    }
    needs_scroll_to_cursor_ = true;
}

// ── Public editing actions (Undo / Redo / Clipboard) ───────────────────────

void EditorView::Undo() {
    if (!buffer_) return;
    buffer_->Undo();
    for (auto& c : cursors_.cursors) {
        c.position.line = std::clamp(c.position.line, 0, buffer_->GetLineCount() - 1);
        int max_col = static_cast<int>(buffer_->GetLine(c.position.line).size());
        c.position.column = std::clamp(c.position.column, 0, max_col);
        c.ClearSelection();
    }
    needs_scroll_to_cursor_ = true;
    ac_open_ = false;
}

void EditorView::Redo() {
    if (!buffer_) return;
    buffer_->Redo();
    for (auto& c : cursors_.cursors) {
        c.position.line = std::clamp(c.position.line, 0, buffer_->GetLineCount() - 1);
        int max_col = static_cast<int>(buffer_->GetLine(c.position.line).size());
        c.position.column = std::clamp(c.position.column, 0, max_col);
        c.ClearSelection();
    }
    needs_scroll_to_cursor_ = true;
    ac_open_ = false;
}

// ── Editing ───────────────────────────────────────────────────────────────

void EditorView::InsertCharAtCursors(const std::string& text) {
    buffer_->BeginUndoGroup();
    for (auto& c : cursors_.cursors) {
        if (c.HasSelection()) DeleteSelection(c);
        int start_line = c.position.line;
        int start_col  = c.position.column;
        buffer_->InsertText(start_line, start_col, text);

        // Calculate end cursor position even when text contains newlines
        int end_line = start_line;
        int end_col  = start_col;
        for (char ch : text) {
            if (ch == '\n') {
                end_line++;
                end_col = 0;
            } else {
                end_col++;
            }
        }
        c.position.line   = end_line;
        c.position.column = end_col;
        c.ClearSelection();
    }
    buffer_->EndUndoGroup();
    needs_scroll_to_cursor_ = true;
}

void EditorView::DeleteAtCursors(bool forward) {
    buffer_->BeginUndoGroup();
    for (auto& c : cursors_.cursors) {
        if (c.HasSelection()) {
            DeleteSelection(c);
            continue;
        }
        if (forward) {
            int line_len = static_cast<int>(buffer_->GetLine(c.position.line).size());
            if (c.position.column < line_len) {
                buffer_->DeleteRange(c.position.line, c.position.column,
                                     c.position.line, c.position.column + 1);
            } else if (c.position.line < buffer_->GetLineCount() - 1) {
                buffer_->DeleteRange(c.position.line, c.position.column,
                                     c.position.line + 1, 0);
            }
        } else {
            // Backspace.
            if (c.position.column > 0) {
                buffer_->DeleteRange(c.position.line, c.position.column - 1,
                                     c.position.line, c.position.column);
                c.position.column--;
                c.ClearSelection();
            } else if (c.position.line > 0) {
                int prev_len = static_cast<int>(buffer_->GetLine(c.position.line - 1).size());
                buffer_->DeleteRange(c.position.line - 1, prev_len,
                                     c.position.line, 0);
                c.position.line--;
                c.position.column = prev_len;
                c.ClearSelection();
            }
        }
    }
    buffer_->EndUndoGroup();
    needs_scroll_to_cursor_ = true;
}

/// Delete the word before (forward=false) or after (forward=true) each cursor.
void EditorView::DeleteWordAtCursors(bool forward) {
    buffer_->BeginUndoGroup();
    for (auto& c : cursors_.cursors) {
        if (c.HasSelection()) { DeleteSelection(c); continue; }
        if (forward) {
            int boundary = GetWordBoundaryRight(c.position.line, c.position.column);
            buffer_->DeleteRange(c.position.line, c.position.column,
                                 c.position.line, boundary);
        } else {
            int boundary = GetWordBoundaryLeft(c.position.line, c.position.column);
            buffer_->DeleteRange(c.position.line, boundary,
                                 c.position.line, c.position.column);
            c.position.column = boundary;
            c.ClearSelection();
        }
    }
    buffer_->EndUndoGroup();
}

/// Insert a newline at each cursor, preserving the previous line's indent.
void EditorView::InsertNewLine() {
    buffer_->BeginUndoGroup();
    for (auto& c : cursors_.cursors) {
        if (c.HasSelection()) DeleteSelection(c);
        int line = c.position.line;
        int col = c.position.column;
        
        std::string indent = GetAutoIndent(line);
        const std::string& current_line = buffer_->GetLine(line);
        std::string extra_indent = use_spaces ? std::string(tab_size, ' ') : "\t";
        
        bool between_braces = (col > 0 && col < static_cast<int>(current_line.length()) && 
                               current_line[col-1] == '{' && current_line[col] == '}');
        
        // Detect if line before cursor ends with ':' or '{' (ignoring whitespace before cursor)
        bool indent_next_line = false;
        int check_idx = col - 1;
        while (check_idx >= 0 && (current_line[check_idx] == ' ' || current_line[check_idx] == '\t')) {
            check_idx--;
        }
        if (check_idx >= 0 && (current_line[check_idx] == ':' || current_line[check_idx] == '{')) {
            indent_next_line = true;
        }
        
        if (between_braces) {
            std::string text_to_insert = "\n" + indent + extra_indent + "\n" + indent;
            buffer_->InsertText(line, col, text_to_insert);
            c.position.line++;
            c.position.column = static_cast<int>(indent.size() + extra_indent.size());
        } else if (indent_next_line) {
            std::string new_indent = indent + extra_indent;
            buffer_->InsertText(line, col, "\n" + new_indent);
            c.position.line++;
            c.position.column = static_cast<int>(new_indent.size());
        } else {
            buffer_->InsertText(line, col, "\n" + indent);
            c.position.line++;
            c.position.column = static_cast<int>(indent.size());
        }
        
        c.ClearSelection();
    }
    buffer_->EndUndoGroup();
    needs_scroll_to_cursor_ = true;
}

/// Insert a tab (spaces or actual tab character) or outdent on Shift+Tab, with Emmet abbreviation expansion.
void EditorView::HandleTab(bool shift) {
    if (!shift && cursors_.cursors.size() == 1 && !cursors_.Primary().HasSelection()) {
        auto& c = cursors_.Primary();
        const auto& line = buffer_->GetLine(c.position.line);
        int col = c.position.column;

        // Extract abbreviation before cursor
        int start = col;
        while (start > 0 && (std::isalnum(static_cast<unsigned char>(line[start - 1])) ||
                             line[start - 1] == '!' || line[start - 1] == '.' ||
                             line[start - 1] == '#' || line[start - 1] == '>' ||
                             line[start - 1] == '+' || line[start - 1] == ':')) {
            --start;
        }

        if (start < col) {
            std::string abbr = line.substr(start, col - start);
            std::string expansion;

            if (abbr == "!" || abbr == "html:5") {
                expansion = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n    <meta charset=\"UTF-8\">\n    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n    <title>Document</title>\n</head>\n<body>\n    \n</body>\n</html>";
            } else if (abbr == "div") {
                expansion = "<div></div>";
            } else if (abbr.starts_with(".") || abbr.starts_with("div.")) {
                std::string cls = abbr.substr(abbr.find('.') + 1);
                expansion = "<div class=\"" + cls + "\"></div>";
            } else if (abbr.starts_with("#") || abbr.starts_with("div#")) {
                std::string id = abbr.substr(abbr.find('#') + 1);
                expansion = "<div id=\"" + id + "\"></div>";
            } else if (abbr == "p") {
                expansion = "<p></p>";
            } else if (abbr == "a") {
                expansion = "<a href=\"\"></a>";
            } else if (abbr == "button" || abbr == "btn") {
                expansion = "<button></button>";
            } else if (abbr == "input") {
                expansion = "<input type=\"text\" />";
            } else if (abbr == "form") {
                expansion = "<form action=\"\" method=\"post\">\n    \n</form>";
            } else if (abbr == "ul>li" || abbr == "ul") {
                expansion = "<ul>\n    <li></li>\n</ul>";
            } else if (abbr == "ol>li" || abbr == "ol") {
                expansion = "<ol>\n    <li></li>\n</ol>";
            } else if (abbr == "table") {
                expansion = "<table>\n    <tr>\n        <td></td>\n    </tr>\n</table>";
            } else if (abbr == "img") {
                expansion = "<img src=\"\" alt=\"\" />";
            } else if (abbr == "script") {
                expansion = "<script src=\"\"></script>";
            } else if (abbr == "link:css" || abbr == "link") {
                expansion = "<link rel=\"stylesheet\" href=\"style.css\">";
            } else if (abbr == "span") {
                expansion = "<span></span>";
            } else if (abbr == "header") {
                expansion = "<header></header>";
            } else if (abbr == "footer") {
                expansion = "<footer></footer>";
            } else if (abbr == "nav") {
                expansion = "<nav></nav>";
            } else if (abbr == "main") {
                expansion = "<main></main>";
            } else if (abbr == "section") {
                expansion = "<section></section>";
            } else if (abbr == "h1") {
                expansion = "<h1></h1>";
            } else if (abbr == "h2") {
                expansion = "<h2></h2>";
            } else if (abbr == "h3") {
                expansion = "<h3></h3>";
            }

            if (!expansion.empty()) {
                buffer_->BeginUndoGroup();
                int cur_line = c.position.line;
                buffer_->DeleteRange(cur_line, start, cur_line, col);
                buffer_->InsertText(cur_line, start, expansion);

                int end_line = cur_line;
                int end_col  = start;
                for (char ch : expansion) {
                    if (ch == '\n') {
                        end_line++;
                        end_col = 0;
                    } else {
                        end_col++;
                    }
                }
                c.position.line   = end_line;
                c.position.column = end_col;
                c.ClearSelection();
                buffer_->EndUndoGroup();
                needs_scroll_to_cursor_ = true;
                ac_open_ = false;
                return;
            }
        }
    }

    std::string tab_str = use_spaces ? std::string(tab_size, ' ') : "\t";

    buffer_->BeginUndoGroup();
    if (shift) {
        // Outdent: remove leading whitespace (up to tab_size chars).
        for (auto& c : cursors_.cursors) {
            const auto& line = buffer_->GetLine(c.position.line);
            int remove = 0;
            for (int j = 0; j < tab_size && j < static_cast<int>(line.size()); ++j) {
                if (line[j] == ' ') ++remove;
                else if (line[j] == '\t') { ++remove; break; }
                else break;
            }
            if (remove > 0) {
                buffer_->DeleteRange(c.position.line, 0, c.position.line, remove);
                c.position.column = std::max(0, c.position.column - remove);
                c.ClearSelection();
            }
        }
    } else {
        InsertCharAtCursors(tab_str);
    }
    buffer_->EndUndoGroup();
}

// ── Selection helpers ─────────────────────────────────────────────────────

void EditorView::DeleteSelection(Cursor& c) {
    if (!c.HasSelection()) return;
    auto begin = c.SelectionBegin();
    auto end   = c.SelectionEnd();
    buffer_->DeleteRange(begin.line, begin.column, end.line, end.column);
    c.MoveTo(begin);
}

std::string EditorView::GetSelectionText(const Cursor& c) const {
    if (!c.HasSelection()) return {};
    auto begin = c.SelectionBegin();
    auto end   = c.SelectionEnd();

    std::string result;
    for (int line = begin.line; line <= end.line; ++line) {
        const auto& text = buffer_->GetLine(line);
        int start_col = (line == begin.line) ? begin.column : 0;
        int end_col   = (line == end.line)   ? end.column
                                              : static_cast<int>(text.size());
        if (line > begin.line) result += '\n';
        result += text.substr(start_col, end_col - start_col);
    }
    return result;
}

/// Ctrl+D: select the next occurrence of the currently selected word,
/// adding a new cursor at that position.
void EditorView::SelectNextOccurrence() {
    auto& primary = cursors_.Primary();
    std::string word;

    if (primary.HasSelection()) {
        word = GetSelectionText(primary);
    } else {
        // Select the word under the cursor.
        int left  = GetWordBoundaryLeft(primary.position.line, primary.position.column);
        int right = GetWordBoundaryRight(primary.position.line, primary.position.column);
        primary.selection_start = {primary.position.line, left};
        primary.position        = {primary.position.line, right};
        return;
    }

    if (word.empty()) return;

    // Search forward from the last cursor's position.
    auto& last = cursors_.cursors.back();
    int search_line = last.SelectionEnd().line;
    int search_col  = last.SelectionEnd().column;

    for (int i = search_line; i < buffer_->GetLineCount(); ++i) {
        const auto& text = buffer_->GetLine(i);
        size_t start = (i == search_line) ? search_col : 0;
        size_t pos   = text.find(word, start);
        if (pos != std::string::npos) {
            Cursor nc;
            nc.selection_start = {i, static_cast<int>(pos)};
            nc.position        = {i, static_cast<int>(pos + word.size())};
            cursors_.cursors.push_back(nc);
            return;
        }
    }
}

// ── Find / Replace ────────────────────────────────────────────────────────

void EditorView::OpenFind()    { find_open_ = true; replace_open_ = false; }
void EditorView::OpenReplace() { find_open_ = true; replace_open_ = true; }
void EditorView::CloseFind()   { find_open_ = false; replace_open_ = false; }

void EditorView::FindNext() {
    std::string query(find_buf_);
    if (query.empty()) return;

    int start_line = cursors_.Primary().position.line;
    int start_col  = cursors_.Primary().position.column;

    for (int i = start_line; i < buffer_->GetLineCount(); ++i) {
        const auto& text = buffer_->GetLine(i);
        size_t from = (i == start_line) ? start_col : 0;
        size_t pos  = text.find(query, from);
        if (pos != std::string::npos) {
            cursors_.ResetToSingle();
            cursors_.Primary().selection_start = {i, static_cast<int>(pos)};
            cursors_.Primary().position        = {i, static_cast<int>(pos + query.size())};
            find_line_ = i;
            find_col_  = static_cast<int>(pos);
            return;
        }
    }
    // Wrap around.
    for (int i = 0; i <= start_line; ++i) {
        const auto& text = buffer_->GetLine(i);
        size_t pos = text.find(query);
        if (pos != std::string::npos) {
            cursors_.ResetToSingle();
            cursors_.Primary().selection_start = {i, static_cast<int>(pos)};
            cursors_.Primary().position        = {i, static_cast<int>(pos + query.size())};
            return;
        }
    }
}

void EditorView::FindPrev() {
    std::string query(find_buf_);
    if (query.empty()) return;

    int start_line = cursors_.Primary().position.line;
    int start_col  = cursors_.Primary().selection_start.column;

    for (int i = start_line; i >= 0; --i) {
        const auto& text = buffer_->GetLine(i);
        size_t search_end = (i == start_line) ? (start_col > 0 ? start_col - 1 : 0) : text.size();
        size_t pos = text.rfind(query, search_end);
        if (pos != std::string::npos) {
            cursors_.ResetToSingle();
            cursors_.Primary().selection_start = {i, static_cast<int>(pos)};
            cursors_.Primary().position        = {i, static_cast<int>(pos + query.size())};
            return;
        }
    }
}

void EditorView::ReplaceNext() {
    std::string query(find_buf_);
    std::string replacement(replace_buf_);
    if (query.empty()) return;

    auto& c = cursors_.Primary();
    if (c.HasSelection() && GetSelectionText(c) == query) {
        buffer_->BeginUndoGroup();
        DeleteSelection(c);
        buffer_->InsertText(c.position.line, c.position.column, replacement);
        c.position.column += static_cast<int>(replacement.size());
        c.ClearSelection();
        buffer_->EndUndoGroup();
    }
    FindNext();
}

void EditorView::ReplaceAll() {
    std::string query(find_buf_);
    std::string replacement(replace_buf_);
    if (query.empty()) return;

    buffer_->BeginUndoGroup();
    for (int i = 0; i < buffer_->GetLineCount(); ++i) {
        const auto& text = buffer_->GetLine(i);
        size_t pos = 0;
        while ((pos = text.find(query, pos)) != std::string::npos) {
            buffer_->DeleteRange(i, static_cast<int>(pos), i, static_cast<int>(pos + query.size()));
            buffer_->InsertText(i, static_cast<int>(pos), replacement);
            pos += replacement.size();
        }
    }
    buffer_->EndUndoGroup();
}

void EditorView::SelectAll() {
    cursors_.ResetToSingle();
    cursors_.Primary().selection_start = {0, 0};
    int last_line = buffer_->GetLineCount() - 1;
    cursors_.Primary().position = {last_line,
        static_cast<int>(buffer_->GetLine(last_line).size())};
}

/// Toggle line comment (prepend/remove "// ") for each cursor's line.
void EditorView::ToggleComment() {
    buffer_->BeginUndoGroup();
    for (auto& c : cursors_.cursors) {
        const auto& line = buffer_->GetLine(c.position.line);
        // Find first non-whitespace character.
        int first_non_ws = 0;
        while (first_non_ws < static_cast<int>(line.size()) &&
               std::isspace(static_cast<unsigned char>(line[first_non_ws])))
            ++first_non_ws;

        if (line.substr(first_non_ws, 3) == "// ") {
            buffer_->DeleteRange(c.position.line, first_non_ws,
                                 c.position.line, first_non_ws + 3);
            c.position.column = std::max(0, c.position.column - 3);
        } else if (line.substr(first_non_ws, 2) == "//") {
            buffer_->DeleteRange(c.position.line, first_non_ws,
                                 c.position.line, first_non_ws + 2);
            c.position.column = std::max(0, c.position.column - 2);
        } else {
            buffer_->InsertText(c.position.line, first_non_ws, "// ");
            c.position.column += 3;
        }
        c.ClearSelection();
    }
    buffer_->EndUndoGroup();
}

// ── Clipboard ─────────────────────────────────────────────────────────────

void EditorView::Copy() {
    std::string text = GetSelectionText(cursors_.Primary());
    if (!text.empty()) {
        ImGui::SetClipboardText(text.c_str());
    }
}

void EditorView::Cut() {
    Copy();
    buffer_->BeginUndoGroup();
    for (auto& c : cursors_.cursors) {
        DeleteSelection(c);
    }
    buffer_->EndUndoGroup();
    needs_scroll_to_cursor_ = true;
}

void EditorView::Paste() {
    const char* clip = ImGui::GetClipboardText();
    if (!clip) return;
    std::string text(clip);
    // Normalise line endings.
    std::erase(text, '\r');
    if (text.empty()) return;

    buffer_->BeginUndoGroup();
    for (auto& c : cursors_.cursors) {
        if (c.HasSelection()) DeleteSelection(c);
        buffer_->InsertText(c.position.line, c.position.column, text);
        // Advance cursor past the pasted text.
        for (char ch : text) {
            if (ch == '\n') { c.position.line++; c.position.column = 0; }
            else            { c.position.column++; }
        }
        c.ClearSelection();
    }
    buffer_->EndUndoGroup();
    needs_scroll_to_cursor_ = true;
}

// ── Scroll ────────────────────────────────────────────────────────────────

/// Scroll the view so that the primary cursor is visible.
void EditorView::EnsureCursorVisible() {
    float lh = ImGui::GetTextLineHeightWithSpacing();
    float cw = ImGui::CalcTextSize("A").x;
    float gw = CalculateGutterWidth();

    float cursor_y = cursors_.Primary().position.line * lh;
    // Horizontal pixel offset of cursor from start of content (includes gutter).
    // This is comparable directly to scroll_x in the canvas coordinate system.
    float cursor_x = gw + cursors_.Primary().position.column * cw;

    float scroll_y = ImGui::GetScrollY();
    float scroll_x = ImGui::GetScrollX();
    float win_h    = ImGui::GetWindowHeight();
    float win_w    = ImGui::GetWindowWidth();

    // ── Vertical ──────────────────────────────────────────────────────────
    if (cursor_y < scroll_y)
        ImGui::SetScrollY(cursor_y);
    else if (cursor_y + lh > scroll_y + win_h)
        ImGui::SetScrollY(cursor_y + lh - win_h);

    // ── Horizontal ────────────────────────────────────────────────────────
    // cursor_x = gw + col*cw.  scroll_x is "how far into the canvas we've scrolled".
    // Text at col c is at screen_x = window_left + padding + gw + col*cw - scroll_x.
    // For text at col c to be visible in the text area [gw, win_w]:
    //   col*cw - (win_w - gw) ≤ scroll_x ≤ col*cw
    // i.e.  col*cw - text_area_w ≤ scroll_x ≤ col*cw
    constexpr float kPad = 8.0f; // px so cursor isn't flush against edge
    if (cursor_x < scroll_x + gw + kPad)
        // Cursor is behind (or near) the gutter — scroll left
        ImGui::SetScrollX(std::max(0.0f, cursor_x - gw - kPad));
    else if (cursor_x > scroll_x + win_w - kPad)
        // Cursor is past the right edge — scroll right.
        // cursor_x = gw + col*cw; expected new scroll = col*cw - text_area_w + kPad
        //   = (cursor_x - gw) - (win_w - gw) + kPad = cursor_x - win_w + kPad
        ImGui::SetScrollX(cursor_x - win_w + kPad);
}

// ── Utility ───────────────────────────────────────────────────────────────

float EditorView::CalculateGutterWidth() const {
    int digits = 1;
    int lines  = buffer_->GetLineCount();
    while (lines >= 10) { ++digits; lines /= 10; }
    digits = std::max(digits, 3);
    return ImGui::CalcTextSize("0").x * (digits + 2) + 8.0f;
}

TextPosition EditorView::ScreenToTextPosition(ImVec2 origin, ImVec2 mouse,
                                               float lh, float cw,
                                               float gw) const {
    int line = static_cast<int>((mouse.y - origin.y) / lh);
    line = std::clamp(line, 0, buffer_->GetLineCount() - 1);

    int col = static_cast<int>((mouse.x - origin.x - gw + cw * 0.5f) / cw);
    col = std::clamp(col, 0, static_cast<int>(buffer_->GetLine(line).size()));

    return {line, col};
}

int EditorView::GetWordBoundaryLeft(int line, int col) const {
    const auto& text = buffer_->GetLine(line);
    if (col <= 0) return 0;
    int i = col - 1;
    // Skip whitespace.
    while (i > 0 && std::isspace(static_cast<unsigned char>(text[i]))) --i;
    // Skip word characters.
    bool is_ident = std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_';
    if (is_ident) {
        while (i > 0 && (std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '_'))
            --i;
    } else {
        // Skip punctuation.
        while (i > 0 && !std::isalnum(static_cast<unsigned char>(text[i - 1])) &&
               text[i - 1] != '_' && !std::isspace(static_cast<unsigned char>(text[i - 1])))
            --i;
    }
    return i;
}

int EditorView::GetWordBoundaryRight(int line, int col) const {
    const auto& text = buffer_->GetLine(line);
    int len = static_cast<int>(text.size());
    if (col >= len) return len;
    int i = col;
    // Skip whitespace.
    while (i < len && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    // Skip word characters.
    bool is_ident = i < len && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_');
    if (is_ident) {
        while (i < len && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
            ++i;
    } else {
        while (i < len && !std::isalnum(static_cast<unsigned char>(text[i])) &&
               text[i] != '_' && !std::isspace(static_cast<unsigned char>(text[i])))
            ++i;
    }
    return i;
}

/// Return the leading whitespace of the given line (for auto-indent).
std::string EditorView::GetAutoIndent(int line) const {
    const auto& text = buffer_->GetLine(line);
    std::string indent;
    for (char c : text) {
        if (c == ' ' || c == '\t') indent += c;
        else break;
    }
    return indent;
}

void EditorView::GoToLine(int line) {
    line = std::clamp(line, 0, buffer_->GetLineCount() - 1);
    cursors_.ResetToSingle();
    cursors_.Primary().MoveTo({line, 0});
    cursor_blink_time_ = 0.0;
    needs_scroll_to_cursor_ = true;
}

// ── Autocomplete / IntelliSense popup ──────────────────────────────────────

namespace {

/// Extract all identifiers declared in the given line that look like a
/// function/method or variable name (as opposed to plain keywords).
/// Returns a list of {name, category} pairs where category is
/// "fn" (function/method) or "var" (variable / type).
struct AcSymbol { std::string name; std::string kind; };

bool IsDeclarationLine(const std::string& line, const std::string& word, AcSymbol& out) {
    // Detect function-like declaration: "type name(" or "type& name("
    auto paren_pos = line.find(word + "(");
    if (paren_pos != std::string::npos) {
        out = {word, "fn"};
        return true;
    }
    // Detect common variable/type declarations: ends with word followed by ; or =
    auto eq_pos   = line.find(word + " =");
    auto semi_pos = line.find(word + ";");
    auto space_pos = line.find(" " + word);
    bool has_type = (line.find("int ")    != std::string::npos ||
                     line.find("float ")  != std::string::npos ||
                     line.find("double ") != std::string::npos ||
                     line.find("bool ")   != std::string::npos ||
                     line.find("char ")   != std::string::npos ||
                     line.find("auto ")   != std::string::npos ||
                     line.find("string ") != std::string::npos ||
                     line.find("void ")   != std::string::npos ||
                     line.find("struct ") != std::string::npos ||
                     line.find("class ")  != std::string::npos ||
                     line.find("let ")    != std::string::npos ||
                     line.find("var ")    != std::string::npos ||
                     line.find("const ")  != std::string::npos);
    if (has_type && (eq_pos != std::string::npos || semi_pos != std::string::npos || space_pos != std::string::npos)) {
        out = {word, "var"};
        return true;
    }
    return false;
}

}  // namespace

void EditorView::UpdateAutocomplete() {
    if (!buffer_) { ac_open_ = false; return; }

    const auto& c = cursors_.Primary();
    const auto& line_text = buffer_->GetLine(c.position.line);
    int col = c.position.column;

    // ── #include completion mode ──────────────────────────────────────────
    // Detect if we're inside an #include ".." or #include <..>
    std::string trimmed = line_text;
    // strip leading whitespace
    size_t first = trimmed.find_first_not_of(" \t");
    if (first != std::string::npos) trimmed = trimmed.substr(first);

    bool is_include_line = (trimmed.size() >= 8 && trimmed.substr(0, 8) == "#include");
    if (is_include_line && col > 0) {
        // Find the opening delimiter: '"' or '<'
        size_t dq = line_text.rfind('"', col - 1);
        size_t lt = line_text.rfind('<',  col - 1);
        size_t delim = std::string::npos;
        bool use_angle = false;
        if (dq != std::string::npos) delim = dq;
        if (lt != std::string::npos && (delim == std::string::npos || lt > dq)) {
            delim = lt;
            use_angle = true;
        }
        // Only trigger if cursor is after the opening delimiter and before any closing one
        if (delim != std::string::npos && delim < static_cast<size_t>(col)) {
            // Check that there's no closing delimiter between delim and col
            char close_ch = use_angle ? '>' : '"';
            bool has_close = false;
            for (size_t i = delim + 1; i < static_cast<size_t>(col); ++i) {
                if (line_text[i] == close_ch) { has_close = true; break; }
            }
            if (!has_close) {
                ac_prefix_ = line_text.substr(delim + 1, col - delim - 1);
                ac_include_mode_ = true;

                // Determine base search directory
                namespace fs = std::filesystem;
                fs::path base_dir;
                if (!current_file_path_.empty()) {
                    base_dir = fs::path(current_file_path_).parent_path();
                } else {
                    base_dir = fs::current_path();
                }

                std::string lower_prefix = ac_prefix_;
                std::ranges::transform(lower_prefix, lower_prefix.begin(), ::tolower);

                std::set<std::string> header_files;
                
                // If using angle brackets, suggest standard C++ libraries
                if (use_angle) {
                    static const std::vector<std::string> kStdLibs = {
                        "iostream", "string", "vector", "map", "set", "unordered_map", "unordered_set",
                        "memory", "algorithm", "cmath", "chrono", "thread", "mutex", "atomic",
                        "fstream", "sstream", "iomanip", "utility", "optional", "variant", "any", "expected",
                        "array", "deque", "list", "forward_list", "stack", "queue", "bitset", "tuple",
                        "format", "ranges", "concepts", "coroutine", "regex", "random", "numeric", "complex"
                    };
                    for (const auto& lib : kStdLibs) {
                        if (lib.starts_with(lower_prefix)) {
                            header_files.insert(lib);
                        }
                    }
                }
                
                std::error_code ec;
                for (auto& entry : fs::recursive_directory_iterator(base_dir, ec)) {
                    if (!entry.is_regular_file(ec)) continue;
                    auto ext = entry.path().extension().string();
                    if (ext != ".h" && ext != ".hpp" && ext != ".hxx" && ext != ".inl") continue;
                    // Get relative path from base_dir
                    auto rel = fs::relative(entry.path(), base_dir, ec);
                    if (ec) continue;
                    std::string rel_str = rel.string();
                    std::ranges::replace(rel_str, '\\', '/');
                    std::string lower_rel = rel_str;
                    std::ranges::transform(lower_rel, lower_rel.begin(), ::tolower);
                    if (lower_rel.starts_with(lower_prefix)) {
                        header_files.insert(rel_str);
                    }
                    if (header_files.size() >= 30) break;
                }

                if (header_files.empty()) {
                    ac_open_ = false;
                    return;
                }
                ac_suggestions_.assign(header_files.begin(), header_files.end());
                ac_selected_ = std::clamp(ac_selected_, 0, static_cast<int>(ac_suggestions_.size()) - 1);
                ac_open_ = true;
                return;
            }
        }
    }

    ac_include_mode_ = false;

    // ── Normal symbol / keyword completion ───────────────────────────────
    // Scan backwards from cursor for identifier prefix
    int start = col;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(line_text[start - 1])) ||
                         line_text[start - 1] == '_' || line_text[start - 1] == '!' ||
                         line_text[start - 1] == '.' || line_text[start - 1] == '#' ||
                         line_text[start - 1] == ':' || line_text[start - 1] == '>')) {
        --start;
    }

    if (col - start < 1) { // Minimum 1 character to show suggestions
        ac_open_ = false;
        return;
    }

    ac_prefix_ = line_text.substr(start, col - start);
    std::string lower_prefix = ac_prefix_;
    std::ranges::transform(lower_prefix, lower_prefix.begin(), ::tolower);

    // Priority 1: symbols declared in the document (functions first, then vars)
    // We collect them separately to insert with priority.
    std::vector<std::string> fn_symbols;    // declared functions/methods
    std::vector<std::string> var_symbols;   // declared variables/types/classes
    std::set<std::string>    plain_words;   // other words from the document

    int total_lines = buffer_->GetLineCount();
    for (int l = 0; l < total_lines; ++l) {
        const auto& lt = buffer_->GetLine(l);
        int idx = 0;
        int len = static_cast<int>(lt.size());
        while (idx < len) {
            while (idx < len && !std::isalnum(static_cast<unsigned char>(lt[idx])) && lt[idx] != '_') ++idx;
            int w_start = idx;
            while (idx < len && (std::isalnum(static_cast<unsigned char>(lt[idx])) || lt[idx] == '_')) ++idx;
            if (idx - w_start <= static_cast<int>(ac_prefix_.size())) continue;

            std::string w = lt.substr(w_start, idx - w_start);
            if (w == ac_prefix_) continue;  // skip exact match (current word)

            std::string lower_w = w;
            std::ranges::transform(lower_w, lower_w.begin(), ::tolower);
            if (!lower_w.starts_with(lower_prefix)) continue;

            AcSymbol sym;
            if (IsDeclarationLine(lt, w, sym)) {
                if (sym.kind == "fn") fn_symbols.push_back(w);
                else                  var_symbols.push_back(w);
            } else {
                plain_words.insert(w);
            }
        }
    }

    // Priority 2: language keywords
    static const std::vector<std::string> kKeywords = {
        "auto", "bool", "break", "case", "catch", "char", "class", "const", "constexpr", "continue",
        "default", "delete", "do", "double", "else", "enum", "explicit", "export", "extern", "false",
        "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
        "noexcept", "nullptr", "operator", "private", "protected", "public", "register", "reinterpret_cast",
        "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
        "template", "this", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
        "using", "virtual", "void", "volatile", "while",
        // C++ stdlib
        "std", "string", "vector", "map", "set", "unordered_map", "unordered_set", "pair", "tuple",
        "unique_ptr", "shared_ptr", "weak_ptr", "make_unique", "make_shared", "optional", "variant", "expected",
        "cout", "cin", "cerr", "endl", "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        // preprocessor
        "#include", "#define", "#ifndef", "#ifdef", "#endif", "#pragma", "#undef", "#error",
        // Rust
        "fn", "let", "mut", "pub", "impl", "trait", "match", "use", "mod", "crate", "self", "Self", "where", "dyn",
        // JS/TS
        "function", "var", "interface", "type", "async", "await", "import", "from", "export", "document", "window",
        "console", "Promise", "Array", "Object", "undefined", "null", "typeof", "instanceof",
        // Python
        "def", "class", "lambda", "pass", "yield", "assert", "with", "as", "raise", "from", "global", "nonlocal",
        "and", "or", "not", "in", "is", "elif", "else", "if", "for", "while", "break", "continue", "return", "try",
        "except", "finally", "import", "match", "case", "async", "await",
        // Python builtins & common methods
        "str", "strip", "startswith", "split", "splitlines", "join", "replace", "lower", "upper",
        "int", "float", "bool", "list", "dict", "set", "tuple", "bytes", "bytearray",
        "print", "len", "range", "enumerate", "zip", "map", "filter", "open", "input",
        "super", "self", "cls", "isinstance", "issubclass", "hasattr", "getattr", "setattr",
        "None", "True", "False", "Exception", "ValueError", "TypeError", "KeyError", "IndexError",
        "append", "extend", "insert", "pop", "remove", "clear", "count", "index", "keys", "values", "items", "get", "update"
    };

    std::vector<std::string> kw_matches;
    for (const auto& kw : kKeywords) {
        if (kw.size() >= ac_prefix_.size()) {
            std::string lower_kw = kw;
            std::ranges::transform(lower_kw, lower_kw.begin(), ::tolower);
            if (lower_kw.starts_with(lower_prefix)) {
                kw_matches.push_back(kw);
            }
        }
    }

    // Emmet snippets (only in HTML/Web/Markdown files)
    std::vector<std::string> emmet_matches;
    
    std::string ext;
    if (!current_file_path_.empty()) {
        size_t dot = current_file_path_.find_last_of('.');
        if (dot != std::string::npos) ext = current_file_path_.substr(dot);
        std::ranges::transform(ext, ext.begin(), ::tolower);
    }
    bool is_web = (ext == ".html" || ext == ".htm" || ext == ".php" || ext == ".md" || ext == ".markdown");
    
    if (is_web) {
        static const std::vector<std::string> kEmmet = {
            "!", "html:5", "div", "span", "p", "a", "button", "btn", "input",
            "form", "ul", "ul>li", "ol", "ol>li", "table", "img", "script",
            "link", "link:css", "header", "footer", "nav", "main", "section",
            "h1", "h2", "h3"
        };
        for (const auto& em : kEmmet) {
            if (em.size() >= ac_prefix_.size() && em.starts_with(ac_prefix_)) {
                emmet_matches.push_back(em);
            }
        }
    }

    // Build final ordered list: functions > vars > plain doc words > keywords > emmet
    // Deduplicate with seen set.
    std::set<std::string> seen;
    ac_suggestions_.clear();

    auto push_unique = [&](const std::string& s) {
        if (!seen.contains(s)) { seen.insert(s); ac_suggestions_.push_back(s); }
    };

    // Sort each group alphabetically before merging
    std::ranges::sort(fn_symbols);
    fn_symbols.erase(std::unique(fn_symbols.begin(), fn_symbols.end()), fn_symbols.end());
    std::ranges::sort(var_symbols);
    var_symbols.erase(std::unique(var_symbols.begin(), var_symbols.end()), var_symbols.end());
    std::ranges::sort(kw_matches);
    std::ranges::sort(emmet_matches);

    for (const auto& s : fn_symbols)  push_unique(s);
    for (const auto& s : var_symbols) push_unique(s);
    for (const auto& s : plain_words) push_unique(s);
    for (const auto& s : kw_matches)  push_unique(s);
    for (const auto& s : emmet_matches) push_unique(s);

    if (ac_suggestions_.empty()) {
        ac_open_ = false;
        return;
    }

    // Limit to 20 items
    if (ac_suggestions_.size() > 20) ac_suggestions_.resize(20);
    ac_selected_ = std::clamp(ac_selected_, 0, static_cast<int>(ac_suggestions_.size()) - 1);
    ac_open_ = true;
}

void EditorView::ApplyAutocomplete() {
    if (!ac_open_ || ac_suggestions_.empty() || ac_selected_ < 0 || ac_selected_ >= static_cast<int>(ac_suggestions_.size())) {
        ac_open_ = false;
        return;
    }

    const std::string& chosen = ac_suggestions_[ac_selected_];
    
    // Check if it's an Emmet snippet
    std::string expansion;
    if (chosen == "!" || chosen == "html:5") {
        expansion = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n    <meta charset=\"UTF-8\">\n    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n    <title>Document</title>\n</head>\n<body>\n    \n</body>\n</html>";
    } else if (chosen == "div") { expansion = "<div></div>";
    } else if (chosen == "p") { expansion = "<p></p>";
    } else if (chosen == "a") { expansion = "<a href=\"\"></a>";
    } else if (chosen == "button" || chosen == "btn") { expansion = "<button></button>";
    } else if (chosen == "input") { expansion = "<input type=\"text\" />";
    } else if (chosen == "form") { expansion = "<form action=\"\" method=\"post\">\n    \n</form>";
    } else if (chosen == "ul>li" || chosen == "ul") { expansion = "<ul>\n    <li></li>\n</ul>";
    } else if (chosen == "ol>li" || chosen == "ol") { expansion = "<ol>\n    <li></li>\n</ol>";
    } else if (chosen == "table") { expansion = "<table>\n    <tr>\n        <td></td>\n    </tr>\n</table>";
    } else if (chosen == "img") { expansion = "<img src=\"\" alt=\"\" />";
    } else if (chosen == "script") { expansion = "<script src=\"\"></script>";
    } else if (chosen == "link:css" || chosen == "link") { expansion = "<link rel=\"stylesheet\" href=\"style.css\">";
    } else if (chosen == "span") { expansion = "<span></span>";
    } else if (chosen == "header") { expansion = "<header></header>";
    } else if (chosen == "footer") { expansion = "<footer></footer>";
    } else if (chosen == "nav") { expansion = "<nav></nav>";
    } else if (chosen == "main") { expansion = "<main></main>";
    } else if (chosen == "section") { expansion = "<section></section>";
    } else if (chosen == "h1") { expansion = "<h1></h1>";
    } else if (chosen == "h2") { expansion = "<h2></h2>";
    } else if (chosen == "h3") { expansion = "<h3></h3>";
    }

    if (!expansion.empty()) {
        buffer_->BeginUndoGroup();
        for (auto& c : cursors_.cursors) {
            int cur_line = c.position.line;
            int col = c.position.column;
            int start = col - static_cast<int>(ac_prefix_.size());
            
            buffer_->DeleteRange(cur_line, start, cur_line, col);
            buffer_->InsertText(cur_line, start, expansion);

            int end_line = cur_line;
            int end_col  = start;
            for (char ch : expansion) {
                if (ch == '\n') {
                    end_line++;
                    end_col = 0;
                } else {
                    end_col++;
                }
            }
            c.position.line   = end_line;
            c.position.column = end_col;
            c.ClearSelection();
        }
        buffer_->EndUndoGroup();
        needs_scroll_to_cursor_ = true;
    } else {
        buffer_->BeginUndoGroup();
        for (auto& c : cursors_.cursors) {
            int cur_line = c.position.line;
            int col = c.position.column;
            int start = col - static_cast<int>(ac_prefix_.size());
            if (start >= 0) {
                buffer_->DeleteRange(cur_line, start, cur_line, col);
                buffer_->InsertText(cur_line, start, chosen);
                c.position.column = start + static_cast<int>(chosen.size());
                c.ClearSelection();
            }
        }
        buffer_->EndUndoGroup();
        needs_scroll_to_cursor_ = true;
    }
    ac_open_ = false;
}

void EditorView::RenderAutocomplete(ImVec2 origin, float line_height, float char_width, float gutter_width) {
    if (!ac_open_ || ac_suggestions_.empty()) return;

    const auto& c = cursors_.Primary();
    float popup_x = origin.x + gutter_width + c.position.column * char_width;
    float popup_y = origin.y + (c.position.line + 1) * line_height;

    // Measure widest item so the popup grows comfortably to fit
    float max_text_w = 320.0f;
    for (const auto& s : ac_suggestions_) {
        float w = ImGui::CalcTextSize(s.c_str()).x + 90.0f;
        if (w > max_text_w) max_text_w = w;
    }
    max_text_w = std::clamp(max_text_w, 320.0f, 650.0f);

    float row_h = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
    bool need_scrollbar = (ac_suggestions_.size() > 8);
    float target_h;
    if (!need_scrollbar) {
        // Generous vertical room so all items fit cleanly without any scrollbar appearing!
        target_h = static_cast<float>(ac_suggestions_.size()) * row_h + 20.0f;
    } else {
        target_h = 8.5f * row_h + 20.0f;
    }

    ImGui::SetNextWindowPos(ImVec2(popup_x, popup_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(max_text_w, target_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.13f, 0.15f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.32f, 0.32f, 0.38f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4, 2));

    ImGuiWindowFlags flags = ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
    if (!need_scrollbar) {
        flags |= ImGuiWindowFlags_NoScrollbar;
    }

    if (ImGui::Begin("##ac_popup", nullptr, flags)) {
        for (int i = 0; i < static_cast<int>(ac_suggestions_.size()); ++i) {
            bool is_selected = (i == ac_selected_);

            // Determine icon & color based on what kind of suggestion this is
            const std::string& sug = ac_suggestions_[i];
            const char* icon  = "  ";
            ImVec4 icon_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

            if (ac_include_mode_) {
                icon       = " H";
                icon_color = ImVec4(0.4f, 0.8f, 0.9f, 1.0f);
            } else {
                // Heuristic: looks like function if it was in fn_symbols (has parens nearby)
                // We distinguish by checking if name has lowercase+underscore style (common for functions)
                bool looks_like_fn = sug.find('_') != std::string::npos &&
                                     std::islower(static_cast<unsigned char>(sug[0]));
                bool looks_like_kw = std::all_of(sug.begin(), sug.end(),
                    [](char ch){ return std::islower(static_cast<unsigned char>(ch)) || ch == '_'; }) &&
                    sug.size() <= 12;

                if (looks_like_fn) {
                    icon       = " f";
                    icon_color = ImVec4(0.9f, 0.7f, 0.3f, 1.0f);  // amber — function
                } else if (looks_like_kw) {
                    icon       = " k";
                    icon_color = ImVec4(0.6f, 0.5f, 0.9f, 1.0f);  // purple — keyword
                } else {
                    icon       = " v";
                    icon_color = ImVec4(0.4f, 0.85f, 0.55f, 1.0f); // green — var/type
                }
            }

            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.04f, 0.38f, 0.65f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.04f, 0.45f, 0.75f, 1.0f));
                ImGui::SetScrollHereY(0.5f);
            }

            // Render icon prefix in colour then the suggestion text
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2);
            ImGui::PushStyleColor(ImGuiCol_Text, icon_color);
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4);

            if (ImGui::Selectable((sug + "##ac" + std::to_string(i)).c_str(), is_selected,
                                   ImGuiSelectableFlags_None, ImVec2(0, 0))) {
                ac_selected_ = i;
                ApplyAutocomplete();
            }

            if (is_selected) {
                ImGui::PopStyleColor(2);
                if (ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
            }
        }
        ImGui::End();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

}  // namespace luce
