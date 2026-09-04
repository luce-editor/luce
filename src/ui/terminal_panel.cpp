// ============================================================================
// TerminalPanel — Implementation.
//
// Manages multiple interactive shell tabs, each with its own process and
// libvterm instance. The UI renders the terminal grid exactly as parsed by libvterm.
// ============================================================================

#include "terminal_panel.h"
#include "imgui.h"

#include <vterm.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace luce {

namespace {

std::string DefaultShellCommand() {
#ifdef _WIN32
    return "pwsh.exe";
#else
    const char* shell = getenv("SHELL");
    return shell ? std::string(shell) : "/bin/bash";
#endif
}

std::string DefaultTabTitle() {
#ifdef _WIN32
    return "pwsh";
#else
    return "sh";
#endif
}

} // namespace

void TerminalPanel::TerminalSession::OutputCallback(const char* s, size_t len, void* user) {
    auto* session = static_cast<TerminalPanel::TerminalSession*>(user);
    session->process.Write(std::string(s, len));
}

TerminalPanel::TerminalSession::TerminalSession() {
    vterm = vterm_new(rows, cols);
    vterm_set_utf8((VTerm*)vterm, 1);
    
    VTermState* state = vterm_obtain_state((VTerm*)vterm);
    
    // Custom palette based on VS Code Dark Modern, with user-requested overrides
    struct { uint8_t r, g, b; } palette[16] = {
        {0, 0, 0},       // 0: Black
        {205, 49, 49},   // 1: Red
        {13, 188, 121},  // 2: Green
        {229, 229, 16},  // 3: Yellow
        {36, 114, 200},  // 4: Blue
        {188, 63, 188},  // 5: Magenta
        {229, 229, 229}, // 6: Cyan -> Override to White (User request)
        {229, 229, 229}, // 7: White
        {102, 102, 102}, // 8: Bright Black -> #666666 for visible PSReadLine suggestions
        {241, 76, 76},   // 9: Bright Red
        {35, 209, 139},  // 10: Bright Green
        {245, 245, 67},  // 11: Bright Yellow
        {59, 142, 234},  // 12: Bright Blue
        {214, 112, 214}, // 13: Bright Magenta
        {255, 255, 255}, // 14: Bright Cyan -> Override to Bright White (User request)
        {255, 255, 255}  // 15: Bright White
    };
    
    for (int i = 0; i < 16; ++i) {
        VTermColor col;
        vterm_color_rgb(&col, palette[i].r, palette[i].g, palette[i].b);
        vterm_state_set_palette_color(state, i, &col);
    }
    
    vterm_screen = vterm_obtain_screen((VTerm*)vterm);
    vterm_screen_reset((VTermScreen*)vterm_screen, 1);
    
    // Set up output callback to send data back to process
    vterm_output_set_callback((VTerm*)vterm, TerminalSession::OutputCallback, this);
}

TerminalPanel::TerminalSession::~TerminalSession() {
    process.Kill();
    if (vterm) {
        vterm_free((VTerm*)vterm);
    }
}

TerminalPanel::TerminalPanel() = default;
TerminalPanel::~TerminalPanel() = default;

void TerminalPanel::StartShell() {
    if (sessions_.empty()) {
        NewTerminal();
    }
}

void TerminalPanel::NewTerminal() {
    auto session = std::make_unique<TerminalSession>();
    session->id = next_session_id_++;
    session->title = DefaultTabTitle();
    sessions_.push_back(std::move(session));
    active_session_index_ = sessions_.size() - 1;
    request_select_tab_ = true;
}

void TerminalPanel::CloseCurrentTerminal() {
    if (sessions_.empty()) return;

    sessions_[active_session_index_]->process.Kill();
    sessions_.erase(sessions_.begin() + static_cast<std::ptrdiff_t>(active_session_index_));

    if (sessions_.empty()) {
        active_session_index_ = 0;
        return;
    }

    if (active_session_index_ >= sessions_.size()) {
        active_session_index_ = sessions_.size() - 1;
    }
    request_select_tab_ = true;
}

bool TerminalPanel::IsRunning() const {
    return !sessions_.empty() && sessions_[active_session_index_]->process.IsRunning();
}

void TerminalPanel::PollOutput() {
    for (size_t i = 0; i < sessions_.size(); ++i) {
        if (!sessions_[i]->shell_started) continue;

        std::string data = sessions_[i]->process.Read();
        if (!data.empty()) {
            vterm_input_write((VTerm*)sessions_[i]->vterm, data.c_str(), data.size());
            sessions_[i]->needs_scroll_to_bottom = true;
        }
    }
}

void TerminalPanel::StartShellInSession(TerminalSession& session) {
    session.title = DefaultTabTitle();
    std::string cmd = DefaultShellCommand();
    
    bool started = session.process.Start(cmd, session.cols, session.rows, working_dir_);
#ifdef _WIN32
    if (!started && cmd == "pwsh.exe") {
        cmd = "powershell.exe"; // Fallback if pwsh doesn't exist
        started = session.process.Start(cmd, session.cols, session.rows, working_dir_);
    }
#endif

    if (!started) {
        // Failed
        const char* err = "Failed to start shell.\n";
        vterm_input_write((VTerm*)session.vterm, err, strlen(err));
    }
    session.shell_started = true;
}

void TerminalPanel::EnsureShellStarted(TerminalSession& session) {
    if (!session.shell_started) {
        StartShellInSession(session);
    }
}

void TerminalPanel::Render(const Theme& theme) {
    if (!visible) return;

    PollOutput();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.terminal_bg);

    if (sessions_.empty()) {
        NewTerminal();
    }

    // Tab strip.
    if (ImGui::BeginTabBar("##terminal_tabbar", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        for (size_t i = 0; i < sessions_.size(); ++i) {
            bool is_selected = (i == active_session_index_);
            bool keep_open = true;

            std::string display_title;
            if (sessions_.size() > 1) {
                display_title = std::to_string(i + 1) + ": " + sessions_[i]->title;
            } else {
                display_title = sessions_[i]->title;
            }
            std::string label = display_title + "###term_tab_" + std::to_string(sessions_[i]->id);

            ImGuiTabItemFlags flags = (is_selected && request_select_tab_) ? ImGuiTabItemFlags_SetSelected : 0;

            // Highlight active tab text, dim inactive tab text
            ImGui::PushStyleColor(ImGuiCol_Text, is_selected ? theme.tab_active_text : theme.tab_text);
            bool tab_active = ImGui::BeginTabItem(label.c_str(), &keep_open, flags);
            ImGui::PopStyleColor();

            if (tab_active) {
                active_session_index_ = i;

                // Draw 2px blue accent line on top of active tab (VS Code style)
                ImVec2 r_min = ImGui::GetItemRectMin();
                ImVec2 r_max = ImGui::GetItemRectMax();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddLine(ImVec2(r_min.x, r_min.y + 1.0f), ImVec2(r_max.x, r_min.y + 1.0f),
                            IM_COL32(0, 122, 204, 255), 2.0f);

                ImGui::EndTabItem();
            }

            if (!keep_open) {
                sessions_[i]->process.Kill();
                sessions_.erase(sessions_.begin() + static_cast<std::ptrdiff_t>(i));
                if (sessions_.empty()) {
                    active_session_index_ = 0;
                    break;
                }
                if (active_session_index_ >= sessions_.size()) {
                    active_session_index_ = sessions_.size() - 1;
                }
                request_select_tab_ = true;
                --i;
            }
        }

        request_select_tab_ = false;

        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
            NewTerminal();
        }

        ImGui::EndTabBar();
    }

    if (sessions_.empty()) {
        ImGui::PopStyleColor();
        return;
    }

    TerminalSession& current = *sessions_[active_session_index_];

    ImGui::PushStyleColor(ImGuiCol_Text, theme.terminal_fg);
    
    // Begin actual terminal render area
    if (ImGui::BeginChild("##terminal_content", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        
        ImVec2 size = ImGui::GetContentRegionAvail();
        float char_width = ImGui::CalcTextSize("A").x;
        float line_height = ImGui::GetTextLineHeight();
        
        int new_cols = std::max(1, static_cast<int>(size.x / char_width));
        int new_rows = std::max(1, static_cast<int>(size.y / line_height));
        
        if (new_cols != current.cols || new_rows != current.rows) {
            current.cols = new_cols;
            current.rows = new_rows;
            vterm_set_size((VTerm*)current.vterm, current.rows, current.cols);
            if (current.shell_started && current.process.IsRunning()) {
                current.process.Resize(current.cols, current.rows);
            }
        }
        
        EnsureShellStarted(current);
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        
        // Handle input if focused
        if (ImGui::IsWindowFocused() || ImGui::IsWindowHovered()) {
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImGui::SetWindowFocus();
            }
            if (ImGui::IsWindowFocused()) {
                ImGuiIO& io = ImGui::GetIO();
                
                // Text input
                for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                    unsigned int c = io.InputQueueCharacters[n];
                    if (c >= 32 && c < 0x10000) {
                        vterm_keyboard_unichar((VTerm*)current.vterm, c, VTERM_MOD_NONE);
                    }
                }
                
                // Key modifiers
                VTermModifier mod = VTERM_MOD_NONE;
                if (io.KeyCtrl) mod = (VTermModifier)(mod | VTERM_MOD_CTRL);
                if (io.KeyAlt) mod = (VTermModifier)(mod | VTERM_MOD_ALT);
                if (io.KeyShift) mod = (VTermModifier)(mod | VTERM_MOD_SHIFT);
                
                // Special keys mapping
                auto map_key = [&](ImGuiKey imgui_key, VTermKey vterm_key) {
                    if (ImGui::IsKeyPressed(imgui_key)) {
                        vterm_keyboard_key((VTerm*)current.vterm, vterm_key, mod);
                    }
                };
                
                map_key(ImGuiKey_Enter, VTERM_KEY_ENTER);
                map_key(ImGuiKey_Tab, VTERM_KEY_TAB);
                map_key(ImGuiKey_Backspace, VTERM_KEY_BACKSPACE);
                map_key(ImGuiKey_Escape, VTERM_KEY_ESCAPE);
                map_key(ImGuiKey_UpArrow, VTERM_KEY_UP);
                map_key(ImGuiKey_DownArrow, VTERM_KEY_DOWN);
                map_key(ImGuiKey_LeftArrow, VTERM_KEY_LEFT);
                map_key(ImGuiKey_RightArrow, VTERM_KEY_RIGHT);
                map_key(ImGuiKey_PageUp, VTERM_KEY_PAGEUP);
                map_key(ImGuiKey_PageDown, VTERM_KEY_PAGEDOWN);
                map_key(ImGuiKey_Home, VTERM_KEY_HOME);
                map_key(ImGuiKey_End, VTERM_KEY_END);
                map_key(ImGuiKey_Insert, VTERM_KEY_INS);
                map_key(ImGuiKey_Delete, VTERM_KEY_DEL);
            }
        }
        
        // Render terminal cells
        VTermScreen* vts = (VTermScreen*)current.vterm_screen;
        
        for (int row = 0; row < current.rows; ++row) {
            for (int col = 0; col < current.cols; ++col) {
                VTermPos pos = { row, col };
                VTermScreenCell cell;
                if (vterm_screen_get_cell(vts, pos, &cell)) {
                    float x = origin.x + col * char_width;
                    float y = origin.y + row * line_height;
                    
                    // Background
                    if (!VTERM_COLOR_IS_DEFAULT_BG(&cell.bg)) {
                        uint8_t br = 0, bg = 0, bb = 0;
                        if (VTERM_COLOR_IS_INDEXED(&cell.bg)) {
                            uint8_t idx = cell.bg.indexed.idx;
                            static const uint8_t pal[16][3] = {
                                {0,0,0}, {205,49,49}, {13,188,121}, {229,229,16},
                                {36,114,200}, {188,63,188}, {229,229,229}, {229,229,229},
                                {102,102,102}, {241,76,76}, {35,209,139}, {245,245,67},
                                {59,142,234}, {214,112,214}, {255,255,255}, {255,255,255}
                            };
                            if (idx < 16) {
                                br = pal[idx][0]; bg = pal[idx][1]; bb = pal[idx][2];
                            } else {
                                br = bg = bb = 0;
                            }
                        } else if (VTERM_COLOR_IS_RGB(&cell.bg)) {
                            br = cell.bg.rgb.red;
                            bg = cell.bg.rgb.green;
                            bb = cell.bg.rgb.blue;
                        }
                        
                        // Ignore extremely dark backgrounds (like ConPTY 12,12,12) to keep UI theme intact
                        if (!(br < 20 && bg < 20 && bb < 20)) {
                            ImU32 bg_color = IM_COL32(br, bg, bb, 255);
                            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + char_width * cell.width, y + line_height), bg_color);
                        }
                    }
                    
                    // Text
                    if (cell.chars[0] != 0 && cell.chars[0] != ' ') {
                        ImU32 fg_color = ImGui::GetColorU32(ImGuiCol_Text);
                        
                        // PSReadLine InlinePrediction is \e[97;2;3m (italic=1, faint/dim=1)
                        if (cell.attrs.italic) {
                            fg_color = IM_COL32(120, 120, 120, 255); // Visible muted gray for predictions!
                        } else if (!VTERM_COLOR_IS_DEFAULT_FG(&cell.fg)) {
                            uint8_t r = 255, g = 255, b = 255;
                            
                            if (VTERM_COLOR_IS_INDEXED(&cell.fg)) {
                                uint8_t idx = cell.fg.indexed.idx;
                                // VS Code Dark Modern palette
                                static const uint8_t pal[16][3] = {
                                    {0,0,0}, {205,49,49}, {13,188,121}, {229,229,16},
                                    {36,114,200}, {188,63,188}, {229,229,229}, {229,229,229}, // 6 and 7 mapped to White
                                    {120,120,120}, {241,76,76}, {35,209,139}, {245,245,67},
                                    {59,142,234}, {214,112,214}, {255,255,255}, {255,255,255} // 14 and 15 mapped to White
                                };
                                if (idx < 16) {
                                    r = pal[idx][0]; g = pal[idx][1]; b = pal[idx][2];
                                } else {
                                    // 256-color fallback (approximate)
                                    r = g = b = 200;
                                }
                            } else if (VTERM_COLOR_IS_RGB(&cell.fg)) {
                                r = cell.fg.rgb.red;
                                g = cell.fg.rgb.green;
                                b = cell.fg.rgb.blue;
                            }
                            
                            // Heuristic to fix PSReadLine colors emitted as TrueColor by ConPTY:
                            // 1. Dark Gray (Suggestions) -> Make it visibly distinct gray
                            if (r == g && g == b && r > 0 && r < 140) {
                                r = g = b = 120; // #787878 - distinct dark gray
                            }
                            // 2. Cyan / Blue / Yellow (Command input / aliases) -> Map to White (User request)
                            else if ((b > 180 && r < 120) || (r > 150 && g > 150 && b < 100)) {
                                r = 255; g = 255; b = 255;
                            }
                            
                            fg_color = IM_COL32(r, g, b, 255);
                        }
                        
                        char utf8_buf[7] = {0};
                        // Very naive unicode to utf8
                        unsigned int c = cell.chars[0];
                        if (c < 0x80) { utf8_buf[0] = c; }
                        else { 
                            // Fallback for simplicity
                            utf8_buf[0] = '?'; 
                        }
                        
                        dl->AddText(ImVec2(x, y), fg_color, utf8_buf);
                    }
                }
            }
        }
        
        // Render Cursor
        VTermState* state = vterm_obtain_state((VTerm*)current.vterm);
        VTermPos cursor_pos;
        vterm_state_get_cursorpos(state, &cursor_pos);
        
        if (ImGui::IsWindowFocused()) {
            float cx = origin.x + cursor_pos.col * char_width;
            float cy = origin.y + cursor_pos.row * line_height;
            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + char_width, cy + line_height), IM_COL32(255, 255, 255, 128));
        }

    }
    ImGui::EndChild();
    ImGui::PopStyleColor(); // Text
    ImGui::PopStyleColor(); // ChildBg
}

}  // namespace luce
