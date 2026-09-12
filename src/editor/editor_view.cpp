// ============================================================================
// EditorView — Implementation.
//
// The editor is rendered as a single ImGui child window with custom drawing.
// Text is drawn token-by-token via ImDrawList for syntax colouring.
// Only visible lines are processed (virtual scrolling).
// ============================================================================

#include "editor_view.h"
#include "diagnostic.h"
#include "editor/symbol_index.h"
#include "editor/git_manager.h"

#include "imgui.h"
#include "imgui_internal.h"
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

    hovered_symbol_.reset();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme_->background);
    ImGui::BeginChild(id, ImVec2(0, 0), false,
        ImGuiWindowFlags_HorizontalScrollbar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNavInputs);

    focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    if (needs_focus_) {
        ImGui::SetWindowFocus();
        focused_ = true;
        needs_focus_ = false;
    }

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

    git_diff_timer_ -= ImGui::GetIO().DeltaTime;
    if (git_diff_timer_ <= 0.0f) {
        git_diff_timer_ = 2.0f;
        RefreshGitDiff();
    }

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

    last_origin_ = origin;
    last_line_height_ = line_height;
    last_char_width_ = char_width;
    last_gutter_width_ = gutter_width;

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

    // Symbol hover & Ctrl+Click navigation
    RenderSymbolHoverAndNavigation(dl, origin, line_height, char_width, gutter_width);

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

    // Set mouse cursor to I-beam when hovering over text area
    if (ImGui::IsWindowHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    }

    // Input handling — active when the editor has focus, or hovered for navigation shortcuts
    bool is_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    if (focused_) {
        HandleMouseInput(origin, line_height, char_width, gutter_width);
        HandleKeyboardInput();
        HandleTextInput();
    } else if (is_hovered) {
        HandleMouseInput(origin, line_height, char_width, gutter_width);
        if (ImGui::IsKeyPressed(ImGuiKey_F12) || (ImGui::GetIO().KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))) {
            GoToDefinition();
        }
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

    // Minimap (rendered on top along the right edge of editor)
    if (show_minimap) {
        RenderMinimap(dl, origin, line_height, char_width, total_lines, first_line, last_line, gutter_width);
    } else {
        // Toggle button in the top-right corner to show minimap
        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();
        ImVec2 btn_pos(win_pos.x + win_size.x - 36.0f, win_pos.y + 6.0f);
        ImGui::SetCursorScreenPos(btn_pos);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.22f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(theme_->gutter_fg.x, theme_->gutter_fg.y, theme_->gutter_fg.z, 0.65f));
        if (ImGui::SmallButton("[|]##show_minimap")) {
            show_minimap = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show Minimap (Ctrl+M)");
        }
        ImGui::PopStyleColor(4);
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextWindow("##editor_context_menu", ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("Go to Definition", "F12 / Ctrl+Click / Ctrl+Enter")) {
            GoToDefinition();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Cut", "Ctrl+X")) { Cut(); }
        if (ImGui::MenuItem("Copy", "Ctrl+C")) { Copy(); }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) { Paste(); }
        ImGui::Separator();
        if (ImGui::MenuItem("Toggle Comment", "Ctrl+/")) { ToggleComment(); }
        ImGui::Separator();
        if (ImGui::MenuItem("Minimap", "Ctrl+M", show_minimap)) {
            show_minimap = !show_minimap;
        }
        ImGui::EndPopup();
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

    float strip_x = origin.x + scroll_x + gw - 7.0f;
    float strip_w = 3.0f;

    for (int i = first; i < last; ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", i + 1);
        float text_width = ImGui::CalcTextSize(buf).x;
        float x = origin.x + scroll_x + gw - text_width - 12.0f;
        float y = origin.y + i * lh;
        dl->AddText(ImVec2(x, y), (i == cursor_line) ? active_fg : fg, buf);

        // Git Gutter diff marker (Added, Modified, Deleted)
        auto it = git_diff_marks_.lines.find(i);
        if (it != git_diff_marks_.lines.end()) {
            if (it->second == GitLineDiffType::Added) {
                // Vibrant green bar
                dl->AddRectFilled(ImVec2(strip_x, y), ImVec2(strip_x + strip_w, y + lh), IM_COL32(73, 208, 130, 255));
            } else if (it->second == GitLineDiffType::Modified) {
                // Vibrant blue bar
                dl->AddRectFilled(ImVec2(strip_x, y), ImVec2(strip_x + strip_w, y + lh), IM_COL32(76, 161, 254, 255));
            } else if (it->second == GitLineDiffType::Deleted) {
                // Small red triangle marker pointing right
                dl->AddTriangleFilled(ImVec2(strip_x - 1.0f, y),
                                      ImVec2(strip_x + strip_w + 1.0f, y),
                                      ImVec2(strip_x - 1.0f, y + 4.0f),
                                      IM_COL32(248, 81, 73, 255));
            }
        }
    }
}

void EditorView::RefreshGitDiff() {
    if (!current_file_path_.empty()) {
        git_diff_marks_ = GitManager::Instance().GetFileDiffMarks(current_file_path_);
    } else {
        git_diff_marks_.lines.clear();
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
        if (ImGui::BeginTooltip()) {
            ImGui::TextUnformatted(hovered_msg.c_str());
            ImGui::Separator();
            if (ImGui::SmallButton("Copy Message")) {
                ImGui::SetClipboardText(hovered_msg.c_str());
            }
            ImGui::EndTooltip();
        }
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
            const std::string& line_str = buffer_->GetLine(line);
            int start_col = (line == begin.line) ? begin.column : 0;
            int end_col   = (line == end.line)   ? end.column
                                                  : static_cast<int>(line_str.size());

            int s_col = std::min(start_col, static_cast<int>(line_str.size()));
            int e_col = std::min(end_col, static_cast<int>(line_str.size()));

            float x1 = origin.x + gw + ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, line_str.c_str(), line_str.c_str() + s_col).x;
            float x2 = origin.x + gw + ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, line_str.c_str(), line_str.c_str() + e_col).x;
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
        float text_w = 0.0f;
        if (cursor.position.column > 0 && cursor.position.line < buffer_->GetLineCount()) {
            const std::string& line = buffer_->GetLine(cursor.position.line);
            int col = std::min(cursor.position.column, static_cast<int>(line.size()));
            text_w = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, line.c_str(), line.c_str() + col).x;
        }
        float x = origin.x + gw + text_w;
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

// ── Minimap ───────────────────────────────────────────────────────────────

void EditorView::RenderMinimap(ImDrawList* dl, ImVec2 origin, float lh, float cw,
                               int total_lines, int first_line, int last_line, float gw) {
    if (!show_minimap || !buffer_ || total_lines <= 0) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    float scroll_y = ImGui::GetScrollY();

    float minimap_w = 118.0f;
    float scrollbar_w = 16.0f;
    float mm_x1 = win_pos.x + win_size.x - minimap_w - scrollbar_w;
    float mm_x2 = mm_x1 + minimap_w;
    float mm_y1 = win_pos.y;
    float mm_y2 = mm_y1 + win_size.y;

    // Minimap background (sleek translucent panel)
    ImU32 mm_bg = ImColor(
        static_cast<int>(theme_->background.x * 190),
        static_cast<int>(theme_->background.y * 190),
        static_cast<int>(theme_->background.z * 190),
        225);
    dl->AddRectFilled(ImVec2(mm_x1, mm_y1), ImVec2(mm_x2, mm_y2), mm_bg);

    // Subtle left border divider
    ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);
    dl->AddLine(ImVec2(mm_x1, mm_y1), ImVec2(mm_x1, mm_y2), border_col, 1.0f);

    float avail_h = win_size.y;
    float line_pitch = 3.5f;
    float mini_bar_h = 2.0f;

    // Calculate minimap scroll offset if document is taller than window
    float total_mm_h = static_cast<float>(total_lines) * line_pitch;
    float mm_scroll_y = 0.0f;
    if (total_mm_h > avail_h) {
        float max_editor_scroll = std::max(1.0f, ImGui::GetScrollMaxY());
        float scroll_ratio = std::clamp(scroll_y / max_editor_scroll, 0.0f, 1.0f);
        mm_scroll_y = scroll_ratio * (total_mm_h - avail_h);
    }

    // Determine range of lines visible in minimap
    int mm_first_line = std::max(0, static_cast<int>(mm_scroll_y / line_pitch));
    int mm_last_line = std::min(total_lines, static_cast<int>((mm_scroll_y + avail_h) / line_pitch) + 2);

    float char_pitch = 1.8f;
    float pad_x = 7.0f;

    for (int l = mm_first_line; l < mm_last_line; ++l) {
        float y = mm_y1 + static_cast<float>(l) * line_pitch - mm_scroll_y;
        if (y < mm_y1 - line_pitch || y > mm_y2) continue;

        const std::string& line = buffer_->GetLine(l);
        if (line.empty()) continue;

        size_t indent = line.find_first_not_of(" \t");
        if (indent == std::string::npos) continue;

        float start_x = mm_x1 + pad_x + static_cast<float>(indent) * char_pitch;

        if (highlighter_) {
            const auto& tokens = highlighter_->GetTokensForLine(l, line);
            if (tokens.empty()) {
                float tx2 = std::min(mm_x2 - 6.0f, start_x + static_cast<float>(line.size() - indent) * char_pitch);
                ImU32 def_col = ImColor(theme_->gutter_fg.x, theme_->gutter_fg.y, theme_->gutter_fg.z, 0.6f);
                dl->AddRectFilled(ImVec2(start_x, y), ImVec2(tx2, y + mini_bar_h), def_col, 0.5f);
            } else {
                for (const auto& tok : tokens) {
                    if (tok.start + tok.length <= static_cast<int>(indent)) continue;
                    float tx1 = mm_x1 + pad_x + static_cast<float>(tok.start) * char_pitch;
                    float tx2 = tx1 + static_cast<float>(tok.length) * char_pitch;
                    if (tx2 > tx1 + 1.2f) {
                        tx2 -= 1.0f; // 1px space between words creates code structure
                    }
                    tx1 = std::clamp(tx1, mm_x1 + pad_x, mm_x2 - 6.0f);
                    tx2 = std::clamp(tx2, mm_x1 + pad_x, mm_x2 - 6.0f);
                    if (tx2 > tx1) {
                        ImVec4 c = theme_->GetTokenColor(tok.type);
                        ImU32 tcol = ImColor(c.x, c.y, c.z, 0.85f);
                        dl->AddRectFilled(ImVec2(tx1, y), ImVec2(tx2, y + mini_bar_h), tcol, 0.5f);
                    }
                }
            }
        } else {
            float tx2 = std::min(mm_x2 - 6.0f, start_x + static_cast<float>(line.size() - indent) * char_pitch);
            ImU32 def_col = ImColor(theme_->gutter_fg.x, theme_->gutter_fg.y, theme_->gutter_fg.z, 0.6f);
            dl->AddRectFilled(ImVec2(start_x, y), ImVec2(tx2, y + mini_bar_h), def_col, 0.5f);
        }
    }

    // Viewport lens (highlight rectangle representing visible code)
    float vp_y1 = mm_y1 + static_cast<float>(first_line) * line_pitch - mm_scroll_y;
    float vp_y2 = mm_y1 + static_cast<float>(last_line) * line_pitch - mm_scroll_y;
    vp_y1 = std::clamp(vp_y1, mm_y1, mm_y2);
    vp_y2 = std::clamp(vp_y2, mm_y1, mm_y2);

    bool mouse_in_mm = (io.MousePos.x >= mm_x1 && io.MousePos.x <= mm_x2 &&
                        io.MousePos.y >= mm_y1 && io.MousePos.y <= mm_y2);

    ImU32 vp_bg = mouse_in_mm ? ImColor(255, 255, 255, 36) : ImColor(255, 255, 255, 20);
    ImU32 vp_border = mouse_in_mm ? ImColor(255, 255, 255, 85) : ImColor(255, 255, 255, 45);
    dl->AddRectFilled(ImVec2(mm_x1 + 1.0f, vp_y1), ImVec2(mm_x2 - 1.0f, vp_y2), vp_bg, 2.0f);
    dl->AddRect(ImVec2(mm_x1 + 1.0f, vp_y1), ImVec2(mm_x2 - 1.0f, vp_y2), vp_border, 2.0f, 0, 1.0f);

    // Cursor position marker in minimap
    if (!cursors_.cursors.empty()) {
        int cur_line = cursors_.Primary().position.line;
        float cur_y = mm_y1 + static_cast<float>(cur_line) * line_pitch - mm_scroll_y;
        if (cur_y >= mm_y1 && cur_y <= mm_y2) {
            ImU32 cur_col = ImColor(theme_->syntax_function.x, theme_->syntax_function.y, theme_->syntax_function.z, 0.95f);
            dl->AddRectFilled(ImVec2(mm_x1, cur_y), ImVec2(mm_x2, cur_y + 1.5f), cur_col);
        }
    }

    // Top-right close button inside minimap
    ImVec2 close_btn_pos(mm_x2 - 18.0f, mm_y1 + 4.0f);
    ImGui::SetCursorScreenPos(close_btn_pos);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(theme_->gutter_fg.x, theme_->gutter_fg.y, theme_->gutter_fg.z, 0.7f));
    if (ImGui::SmallButton("x##close_mm")) {
        show_minimap = false;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Hide Minimap (Ctrl+M)");
    }
    ImGui::PopStyleColor(4);

    // Handle mouse clicking & dragging inside minimap
    if (mouse_in_mm) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (mouse_in_mm && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        minimap_dragging_ = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        minimap_dragging_ = false;
    }

    if (minimap_dragging_) {
        float rel_y = io.MousePos.y - mm_y1 + mm_scroll_y;
        int target_line = static_cast<int>(rel_y / line_pitch);
        target_line = std::clamp(target_line, 0, total_lines - 1);
        int half_visible = (last_line - first_line) / 2;
        float new_scroll_y = static_cast<float>(target_line - half_visible) * lh;
        ImGui::SetScrollY(std::clamp(new_scroll_y, 0.0f, ImGui::GetScrollMaxY()));
    }
}

// ── Symbol hover & Ctrl+Click navigation ───────────────────────────────────

void EditorView::RenderSymbolHoverAndNavigation(ImDrawList* dl, ImVec2 origin, float lh, float cw, float gw) {
    if (!symbol_index_ || !buffer_) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse_pos = io.MousePos;
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

    float minimap_w = show_minimap ? 110.0f : 0.0f;
    float scrollbar_w = 16.0f;

    // Must be within text bounds (not gutter, not scrollbars, not minimap)
    if (mouse_pos.x < win_pos.x + gw || mouse_pos.x >= win_pos.x + win_size.x - minimap_w - scrollbar_w ||
        mouse_pos.y < win_pos.y || mouse_pos.y >= win_pos.y + win_size.y - scrollbar_w) {
        return;
    }

    TextPosition tpos = ScreenToTextPosition(origin, mouse_pos, lh, cw, gw);
    if (tpos.line < 0 || tpos.line >= buffer_->GetLineCount()) return;

    const std::string& line = buffer_->GetLine(tpos.line);
    int len = static_cast<int>(line.size());
    if (tpos.column < 0 || tpos.column >= len) return;

    char ch = line[tpos.column];
    if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') return;

    int left = tpos.column;
    while (left > 0 && (std::isalnum(static_cast<unsigned char>(line[left - 1])) || line[left - 1] == '_')) {
        left--;
    }
    int right = tpos.column;
    while (right < len && (std::isalnum(static_cast<unsigned char>(line[right])) || line[right] == '_')) {
        right++;
    }
    if (right <= left) return;

    std::string word = line.substr(left, right - left);
    if (word.empty()) return;

    // Do not show symbol hover for C++ language keywords
    static const std::unordered_set<std::string> kKeywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
        "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "override", "private",
        "protected", "public", "register", "reinterpret_cast", "requires", "return",
        "short", "signed", "sizeof", "static", "static_assert", "static_cast",
        "struct", "switch", "template", "this", "thread_local", "throw", "true",
        "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
        "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
    };
    if (kKeywords.contains(word)) return;

    // Suppress symbol hover if this word is at an active compiler/linter error location
    if (!current_file_path_.empty()) {
        std::string norm_path = current_file_path_;
        std::ranges::replace(norm_path, '\\', '/');
        auto diags = DiagnosticManager::Instance().GetDiagnosticsForFile(norm_path);
        for (const auto& d : diags) {
            if (d.line == tpos.line + 1 && d.severity == DiagnosticSeverity::Error) {
                int d_col_start = std::max(0, d.column - 1);
                int d_col_end = d_col_start + 4;
                if (d_col_start < len) {
                    int peek = d_col_start;
                    while (peek < len && line[peek] != ' ' && line[peek] != '\t' && line[peek] != ';') peek++;
                    if (peek > d_col_start) d_col_end = peek;
                }
                if (left <= d_col_end && right >= d_col_start) {
                    return; // Error squiggly tooltip takes priority
                }
            }
        }
    }

    auto sym_opt = symbol_index_->FindSymbol(word, current_file_path_);
    if (!sym_opt.has_value()) return;

    const auto& sym = *sym_opt;

    float x1 = origin.x + gw + static_cast<float>(left) * cw;
    float x2 = origin.x + gw + static_cast<float>(right) * cw;
    float y1 = origin.y + static_cast<float>(tpos.line) * lh;
    float y2 = y1 + lh;

    if (mouse_pos.x >= x1 && mouse_pos.x <= x2 && mouse_pos.y >= y1 && mouse_pos.y <= y2) {
        hovered_symbol_ = sym;
        if (io.KeyCtrl) {
            ImU32 link_col = ImGui::ColorConvertFloat4ToU32(theme_->syntax_function);
            dl->AddLine(ImVec2(x1, y2 - 2.0f), ImVec2(x2, y2 - 2.0f), link_col, 1.5f);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // Direct jump on Ctrl+Click, Ctrl+Enter, or F12 while hovering over this symbol
        if (on_goto_definition_) {
            bool clicked_link = io.KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            bool key_jump = ImGui::IsKeyPressed(ImGuiKey_F12) || 
                            (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)));
            if (clicked_link || key_jump) {
                on_goto_definition_(sym.file_path, sym.line);
                return;
            }
        }

        if (io.KeyCtrl || ImGui::IsWindowHovered()) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(theme_->background.x * 0.85f, theme_->background.y * 0.85f, theme_->background.z * 0.85f, 0.98f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Border));

            if (ImGui::BeginTooltip()) {
                std::string sig = sym.signature.empty() ? (sym.scope.empty() ? sym.name : (sym.scope + "::" + sym.name)) : sym.signature;

                std::istringstream ss(sig);
                std::string tok;
                bool first_tok = true;
                while (ss >> tok) {
                    if (!first_tok) ImGui::SameLine(0, 4.0f);
                    first_tok = false;

                    if (tok == "inline" || tok == "virtual" || tok == "static" || tok == "constexpr" ||
                        tok == "explicit" || tok == "const" || tok == "override" || tok == "final" ||
                        tok == "noexcept" || tok == "class" || tok == "struct" || tok == "function") {
                        ImGui::TextColored(theme_->syntax_keyword, "%s", tok.c_str());
                    } else if (tok == "bool" || tok == "void" || tok == "int" || tok == "float" ||
                               tok == "double" || tok == "char" || tok == "size_t" || tok == "auto" ||
                               tok.starts_with("std::") || tok.starts_with("ImVec") || tok.starts_with("ImFont")) {
                        ImGui::TextColored(theme_->syntax_type, "%s", tok.c_str());
                    } else if (tok.find('(') != std::string::npos) {
                        auto paren = tok.find('(');
                        std::string fn_part = tok.substr(0, paren);
                        std::string rest = tok.substr(paren);
                        ImGui::TextColored(theme_->syntax_function, "%s", fn_part.c_str());
                        ImGui::SameLine(0, 0);
                        ImGui::TextColored(theme_->syntax_punctuation, "%s", rest.c_str());
                    } else {
                        ImGui::TextColored(theme_->foreground, "%s", tok.c_str());
                    }
                }

                if (!sym.doc_comment.empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::TextColored(theme_->gutter_fg, "%s", sym.doc_comment.c_str());
                }

                ImGui::Spacing();
                std::string filename = std::filesystem::path(sym.file_path).filename().string();
                ImGui::TextDisabled("Ctrl+Click or Ctrl+Enter / F12 to go to definition (%s:%d)", filename.c_str(), sym.line);

                ImGui::EndTooltip();
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
    }
}

bool EditorView::GoToDefinition() {
    if (!symbol_index_ || !on_goto_definition_ || !buffer_) return false;

    // 1. If mouse is currently hovering over a symbol that was resolved and highlighted, jump immediately!
    if (hovered_symbol_.has_value()) {
        on_goto_definition_(hovered_symbol_->file_path, hovered_symbol_->line);
        return true;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse_pos = io.MousePos;
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    float minimap_w = show_minimap ? 110.0f : 0.0f;
    float scrollbar_w = 16.0f;

    std::string target_word;

    // 2. Check if mouse is hovering over an identifier in the text area
    bool mouse_in_text = (mouse_pos.x >= win_pos.x + last_gutter_width_ &&
                          mouse_pos.x < win_pos.x + win_size.x - minimap_w - scrollbar_w &&
                          mouse_pos.y >= win_pos.y &&
                          mouse_pos.y < win_pos.y + win_size.y - scrollbar_w);

    if (mouse_in_text && last_line_height_ > 0.0f && last_char_width_ > 0.0f) {
        TextPosition tpos = ScreenToTextPosition(last_origin_, mouse_pos, last_line_height_, last_char_width_, last_gutter_width_);
        if (tpos.line >= 0 && tpos.line < buffer_->GetLineCount()) {
            const auto& line = buffer_->GetLine(tpos.line);
            int len = static_cast<int>(line.size());
            if (tpos.column >= 0 && tpos.column < len &&
                (std::isalnum(static_cast<unsigned char>(line[tpos.column])) || line[tpos.column] == '_')) {
                int left = tpos.column;
                while (left > 0 && (std::isalnum(static_cast<unsigned char>(line[left - 1])) || line[left - 1] == '_')) left--;
                int right = tpos.column;
                while (right < len && (std::isalnum(static_cast<unsigned char>(line[right])) || line[right] == '_')) right++;
                std::string w = line.substr(left, right - left);
                if (!w.empty() && symbol_index_->FindSymbol(w, current_file_path_).has_value()) {
                    target_word = w;
                }
            }
        }
    }

    // 3. If no valid hovered symbol under mouse, check primary cursor
    if (target_word.empty()) {
        const auto& cur = cursors_.Primary().position;
        if (cur.line >= 0 && cur.line < buffer_->GetLineCount()) {
            const auto& line = buffer_->GetLine(cur.line);
            int len = static_cast<int>(line.size());
            int col = cur.column;
            // If cursor is at or past the end of word (e.g. `funk|()` or `funk|;`), look left
            if (col > 0 && (col >= len || (!std::isalnum(static_cast<unsigned char>(line[col])) && line[col] != '_'))) {
                if (std::isalnum(static_cast<unsigned char>(line[col - 1])) || line[col - 1] == '_') {
                    col = col - 1;
                }
            }
            if (col >= 0 && col < len &&
                (std::isalnum(static_cast<unsigned char>(line[col])) || line[col] == '_')) {
                int left = col;
                while (left > 0 && (std::isalnum(static_cast<unsigned char>(line[left - 1])) || line[left - 1] == '_')) left--;
                int right = col;
                while (right < len && (std::isalnum(static_cast<unsigned char>(line[right])) || line[right] == '_')) right++;
                std::string w = line.substr(left, right - left);
                if (!w.empty() && symbol_index_->FindSymbol(w, current_file_path_).has_value()) {
                    target_word = w;
                }
            }
        }
    }

    if (target_word.empty()) return false;

    auto sym = symbol_index_->FindSymbol(target_word, current_file_path_);
    if (sym.has_value() && on_goto_definition_) {
        on_goto_definition_(sym->file_path, sym->line);
        return true;
    }
    return false;
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
    }
    // Go to Definition shortcut: F12
    if (ImGui::IsKeyPressed(ImGuiKey_F12)) {
        GoToDefinition();
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
        UpdateAutocomplete();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (ctrl) DeleteWordAtCursors(true); else DeleteAtCursors(true);
        UpdateAutocomplete();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        if (ctrl) {
            // If on a symbol (hovered or at cursor), jump to definition
            if (!GoToDefinition()) {
                // Otherwise Ctrl+Enter: Insert line below without splitting current line
                MoveCursorEnd(false);
                InsertNewLine();
            }
        } else {
            InsertNewLine();
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Space)) {
        UpdateAutocomplete();
    }
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

    // Check if mouse is on the vertical scrollbar area (right 16px) or horizontal scrollbar (bottom 16px) or minimap
    float minimap_w = show_minimap ? 110.0f : 0.0f;
    bool on_minimap = (io.MousePos.x >= win_pos.x + win_size.x - minimap_w - 16.0f &&
                       io.MousePos.x <= win_pos.x - 16.0f);
    bool on_v_scrollbar = (io.MousePos.x >= win_pos.x + win_size.x - 16.0f);
    bool on_h_scrollbar = (io.MousePos.y >= win_pos.y + win_size.y - 16.0f);

    if (on_v_scrollbar || on_h_scrollbar || on_minimap) {
        return; // Don't drag-select when user interacts with scrollbars or minimap
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        TextPosition pos = ScreenToTextPosition(origin, io.MousePos, lh, cw, gw);

        if (io.KeyCtrl) {
            // Check if Ctrl+Click was on an identifier with Go to Definition
            bool jumped = GoToDefinition();
            if (!jumped) {
                // Ctrl+Click: add cursor.
                cursors_.AddCursor(pos);
            }
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

        char buf[8] = {};
        ImTextCharToUtf8(buf, ch);
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
            const std::string& line = buffer_->GetLine(c.position.line);
            int step = 1;
            while (c.position.column - step > 0 &&
                   (static_cast<unsigned char>(line[c.position.column - step]) & 0xC0) == 0x80) {
                step++;
            }
            c.MoveTo({c.position.line, c.position.column - step}, ext);
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
            const std::string& line = buffer_->GetLine(c.position.line);
            int step = 1;
            while (c.position.column + step < line_len &&
                   (static_cast<unsigned char>(line[c.position.column + step]) & 0xC0) == 0x80) {
                step++;
            }
            c.MoveTo({c.position.line, c.position.column + step}, ext);
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
                const std::string& line = buffer_->GetLine(c.position.line);
                int del_count = 1;
                while (c.position.column + del_count < line_len &&
                       (static_cast<unsigned char>(line[c.position.column + del_count]) & 0xC0) == 0x80) {
                    del_count++;
                }
                buffer_->DeleteRange(c.position.line, c.position.column,
                                     c.position.line, c.position.column + del_count);
            } else if (c.position.line < buffer_->GetLineCount() - 1) {
                buffer_->DeleteRange(c.position.line, c.position.column,
                                     c.position.line + 1, 0);
            }
        } else {
            // Backspace.
            if (c.position.column > 0) {
                const std::string& line = buffer_->GetLine(c.position.line);
                int del_count = 1;
                while (c.position.column - del_count > 0 &&
                       (static_cast<unsigned char>(line[c.position.column - del_count]) & 0xC0) == 0x80) {
                    del_count++;
                }
                buffer_->DeleteRange(c.position.line, c.position.column - del_count,
                                     c.position.line, c.position.column);
                c.position.column -= del_count;
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
        if (c.HasSelection()) {
            DeleteSelection(c);
            continue;
        }
        if (forward) {
            int line_len = static_cast<int>(buffer_->GetLine(c.position.line).size());
            if (c.position.column >= line_len) {
                if (c.position.line < buffer_->GetLineCount() - 1) {
                    // At end of line: merge with next line
                    buffer_->DeleteRange(c.position.line, line_len,
                                         c.position.line + 1, 0);
                    c.ClearSelection();
                }
            } else {
                int boundary = GetWordBoundaryRight(c.position.line, c.position.column);
                if (boundary == c.position.column && c.position.line < buffer_->GetLineCount() - 1) {
                    buffer_->DeleteRange(c.position.line, c.position.column,
                                         c.position.line + 1, 0);
                    c.ClearSelection();
                } else if (boundary > c.position.column) {
                    buffer_->DeleteRange(c.position.line, c.position.column,
                                         c.position.line, boundary);
                    c.ClearSelection();
                }
            }
        } else {
            // Backward (Ctrl+Backspace)
            const auto& line_text = buffer_->GetLine(c.position.line);
            int col = std::min(c.position.column, static_cast<int>(line_text.size()));

            // Check if everything before cursor on this line is whitespace (or col == 0)
            bool only_whitespace = true;
            for (int k = 0; k < col; ++k) {
                if (!std::isspace(static_cast<unsigned char>(line_text[k]))) {
                    only_whitespace = false;
                    break;
                }
            }

            if (only_whitespace && c.position.line > 0) {
                // Line before cursor is only whitespace (or cursor is at col 0):
                // Delete the newline and any leading whitespace on this line,
                // merging back into the previous line.
                int prev_line = c.position.line - 1;
                int prev_len = static_cast<int>(buffer_->GetLine(prev_line).size());
                buffer_->DeleteRange(prev_line, prev_len, c.position.line, col);
                c.position.line = prev_line;
                c.position.column = prev_len;
                c.ClearSelection();
            } else {
                int boundary = GetWordBoundaryLeft(c.position.line, col);
                if (boundary == col && c.position.line > 0) {
                    int prev_line = c.position.line - 1;
                    int prev_len = static_cast<int>(buffer_->GetLine(prev_line).size());
                    buffer_->DeleteRange(prev_line, prev_len, c.position.line, col);
                    c.position.line = prev_line;
                    c.position.column = prev_len;
                    c.ClearSelection();
                } else if (boundary < col) {
                    buffer_->DeleteRange(c.position.line, boundary,
                                         c.position.line, col);
                    c.position.column = boundary;
                    c.ClearSelection();
                }
            }
        }
    }
    buffer_->EndUndoGroup();
    needs_scroll_to_cursor_ = true;
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
    if (!ImGui::GetCurrentContext() || !ImGui::GetCurrentWindowRead()) {
        needs_scroll_to_cursor_ = true;
        return;
    }

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

void EditorView::GoToPosition(int line, int column) {
    if (!buffer_ || buffer_->GetLineCount() == 0) return;
    line = std::clamp(line, 0, buffer_->GetLineCount() - 1);
    int max_col = static_cast<int>(buffer_->GetLine(line).size());
    column = std::clamp(column, 0, max_col);
    cursors_.ResetToSingle();
    cursors_.Primary().MoveTo({line, column});
    cursor_blink_time_ = 0.0;
    needs_scroll_to_cursor_ = true;
    needs_focus_ = true;
}

void EditorView::GoToLine(int line) {
    GoToPosition(line, 0);
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

static std::string ResolveVariableType(const TextBuffer* buffer, int current_line, const std::string& var_name) {
    if (!buffer || var_name.empty() || current_line < 0) return "";

    if (var_name == "this") {
        for (int l = current_line; l >= 0; --l) {
            const auto& line = buffer->GetLine(l);
            static const std::regex re_enc(R"(\b(class|struct)\s+([a-zA-Z0-9_]+))");
            std::smatch m;
            if (std::regex_search(line, m, re_enc)) {
                return m[2].str();
            }
        }
        return "";
    }

    static const std::set<std::string> kNonTypes = {
        "if", "while", "for", "switch", "case", "return", "throw", "else", "goto", "sizeof", "alignas"
    };

    std::string escaped_var;
    for (char c : var_name) {
        if (c == '.' || c == '[' || c == ']' || c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '*' || c == '+' || c == '?' || c == '^' || c == '$' || c == '\\' || c == '|') {
            escaped_var += '\\';
        }
        escaped_var += c;
    }

    // Pattern 1: [Type] [*&] var_name
    std::regex re_decl(R"(\b([a-zA-Z0-9_:]+)(?:<([a-zA-Z0-9_:]+)>)?(?:\s*[*&]+)?\s+)" + escaped_var + R"(\b(?:\s*[\(={\[;,]|\s*$))");

    // Pattern 2: auto [*&] var_name = (new)? Type
    std::regex re_auto(R"(\bauto(?:\s*[*&]+)?\s+)" + escaped_var + R"(\s*=\s*(?:new\s+)?([a-zA-Z0-9_:]+))");

    int start_line = std::max(0, current_line - 300);
    for (int l = current_line; l >= start_line; --l) {
        std::string line = buffer->GetLine(l);
        auto comment_pos = line.find("//");
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);

        std::smatch m;
        if (std::regex_search(line, m, re_auto)) {
            std::string t = m[1].str();
            if (!kNonTypes.contains(t)) return t;
        }

        if (std::regex_search(line, m, re_decl)) {
            std::string base_type = m[1].str();
            std::string templ_type = m[2].str();
            if (base_type == "unique_ptr" || base_type == "shared_ptr" ||
                base_type == "std::unique_ptr" || base_type == "std::shared_ptr") {
                if (!templ_type.empty()) return templ_type;
            }
            if (base_type != "const" && base_type != "static" && base_type != "constexpr" && !kNonTypes.contains(base_type)) {
                return base_type;
            }
        }
    }

    return "";
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

    // ── Member / Scope completion mode (e.g. t. or ptr-> or Type::) ───────
    int member_start = col;
    while (member_start > 0 && (std::isalnum(static_cast<unsigned char>(line_text[member_start - 1])) ||
                                line_text[member_start - 1] == '_')) {
        member_start--;
    }
    std::string member_prefix = line_text.substr(member_start, col - member_start);

    // Look right before member_start for . or -> or ::
    int op_pos = member_start;
    while (op_pos > 0 && (line_text[op_pos - 1] == ' ' || line_text[op_pos - 1] == '\t')) {
        op_pos--;
    }

    bool is_member_op = false;
    bool is_scope_op  = false;
    int  op_len       = 0;

    if (op_pos >= 2 && line_text.substr(op_pos - 2, 2) == "->") {
        is_member_op = true;
        op_len = 2;
    } else if (op_pos >= 2 && line_text.substr(op_pos - 2, 2) == "::") {
        is_scope_op = true;
        op_len = 2;
    } else if (op_pos >= 1 && line_text[op_pos - 1] == '.') {
        is_member_op = true;
        op_len = 1;
    }

    if (is_member_op || is_scope_op) {
        int var_end = op_pos - op_len;
        while (var_end > 0 && (line_text[var_end - 1] == ' ' || line_text[var_end - 1] == '\t')) {
            var_end--;
        }
        int var_start = var_end;
        while (var_start > 0 && (std::isalnum(static_cast<unsigned char>(line_text[var_start - 1])) ||
                                 line_text[var_start - 1] == '_' || line_text[var_start - 1] == ':')) {
            var_start--;
        }

        std::string var_name = line_text.substr(var_start, var_end - var_start);
        if (!var_name.empty()) {
            std::string target_type;
            if (is_scope_op) {
                target_type = var_name;
            } else {
                target_type = ResolveVariableType(buffer_, c.position.line, var_name);
                if (target_type.empty()) {
                    target_type = var_name;
                }
            }

            std::vector<SymbolInfo> members;
            if (symbol_index_ && !target_type.empty()) {
                members = symbol_index_->GetMembersOf(target_type);
            }

            // Also check if the class definition is present directly in current buffer
            if (members.empty() && !target_type.empty()) {
                int tot = buffer_->GetLineCount();
                bool inside_class = false;
                for (int l = 0; l < tot; ++l) {
                    const auto& lt = buffer_->GetLine(l);
                    if (!inside_class) {
                        if (lt.find("class " + target_type) != std::string::npos ||
                            lt.find("struct " + target_type) != std::string::npos) {
                            inside_class = true;
                        }
                    } else {
                        if (lt.find("};") != std::string::npos) {
                            inside_class = false;
                            break;
                        }
                        static const std::regex re_m(R"(\b([a-zA-Z0-9_]+)\s*\([^;{]*\))");
                        std::smatch m;
                        if (std::regex_search(lt, m, re_m)) {
                            std::string fn_name = m[1].str();
                            if (fn_name != target_type && !fn_name.starts_with("~") &&
                                fn_name != "if" && fn_name != "while" && fn_name != "for") {
                                SymbolInfo s;
                                s.name = fn_name;
                                s.kind = SymbolKind::Method;
                                members.push_back(s);
                            }
                        }
                    }
                }
            }

            if (!members.empty()) {
                std::string lower_mem_prefix = member_prefix;
                std::ranges::transform(lower_mem_prefix, lower_mem_prefix.begin(), ::tolower);

                std::vector<std::string> matched_methods;
                std::vector<std::string> matched_vars;
                std::set<std::string> seen_members;

                for (const auto& m : members) {
                    std::string lower_name = m.name;
                    std::ranges::transform(lower_name, lower_name.begin(), ::tolower);
                    if (lower_name.starts_with(lower_mem_prefix)) {
                        if (m.kind == SymbolKind::Method || m.kind == SymbolKind::Function) {
                            std::string item = m.name + "()";
                            if (!seen_members.contains(item)) {
                                seen_members.insert(item);
                                matched_methods.push_back(item);
                            }
                        } else {
                            if (!seen_members.contains(m.name)) {
                                seen_members.insert(m.name);
                                matched_vars.push_back(m.name);
                            }
                        }
                    }
                }

                std::ranges::sort(matched_methods);
                std::ranges::sort(matched_vars);

                ac_suggestions_.clear();
                ac_suggestions_.insert(ac_suggestions_.end(), matched_methods.begin(), matched_methods.end());
                ac_suggestions_.insert(ac_suggestions_.end(), matched_vars.begin(), matched_vars.end());

                if (!ac_suggestions_.empty()) {
                    ac_prefix_ = member_prefix;
                    ac_selected_ = 0;
                    ac_open_ = true;
                    return;
                }
            }
        }
    }

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

    std::vector<std::string> plugin_matches;
    if (completion_provider_) {
        const auto& cur = cursors_.Primary();
        plugin_matches = completion_provider_(ext, ac_prefix_, cur.position.line, cur.position.column);
        std::ranges::sort(plugin_matches);
    }

    for (const auto& s : fn_symbols)      push_unique(s);
    for (const auto& s : var_symbols)     push_unique(s);
    for (const auto& s : plugin_matches)  push_unique(s);
    for (const auto& s : plain_words)     push_unique(s);
    for (const auto& s : kw_matches)      push_unique(s);
    for (const auto& s : emmet_matches)   push_unique(s);

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
                if (chosen.size() >= 2 && chosen.ends_with("()")) {
                    c.position.column = start + static_cast<int>(chosen.size()) - 1;
                } else {
                    c.position.column = start + static_cast<int>(chosen.size());
                }
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
                bool is_method = (sug.size() >= 2 && sug.ends_with("()"));
                bool looks_like_fn = is_method || (sug.find('_') != std::string::npos &&
                                     std::islower(static_cast<unsigned char>(sug[0])));
                bool looks_like_kw = !is_method && std::all_of(sug.begin(), sug.end(),
                    [](char ch){ return std::islower(static_cast<unsigned char>(ch)) || ch == '_'; }) &&
                    sug.size() <= 12;

                if (looks_like_fn) {
                    icon       = is_method ? " m" : " f";
                    icon_color = ImVec4(0.9f, 0.7f, 0.3f, 1.0f);  // amber — method/function
                } else if (looks_like_kw) {
                    icon       = " k";
                    icon_color = ImVec4(0.6f, 0.5f, 0.9f, 1.0f);  // purple — keyword
                } else {
                    icon       = " v";
                    icon_color = ImVec4(0.4f, 0.85f, 0.55f, 1.0f); // green — var/field/type
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
