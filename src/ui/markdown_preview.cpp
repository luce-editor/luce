// ============================================================================
// MarkdownPreview — Implementation.
// ============================================================================

#include "markdown_preview.h"
#include "editor/text_buffer.h"
#include "imgui.h"
#include <sstream>
#include <algorithm>

namespace luce {

// Strip 4-byte UTF-8 sequences (most emojis) and 0xE2... sequences (Dingbats/Symbols like ✨, ✅)
static std::string RemoveEmojis(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = s[i];
        if (c < 0x80) { // ASCII
            out += c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) { // 2 bytes (e.g. Polish characters)
            if (i + 1 < s.size()) out += s.substr(i, 2);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) { // 3 bytes
            // 0xE2 covers U+2000 to U+2FFF (Symbols, Dingbats). We drop them.
            if (c != 0xE2) {
                if (i + 2 < s.size()) out += s.substr(i, 3);
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) { // 4 bytes (SMP emojis like 🚀)
            i += 4;
        } else {
            i += 1; // skip invalid
        }
    }
    return out;
}

MarkdownPreview::MarkdownPreview() = default;

void MarkdownPreview::Render(const char* id, const TextBuffer* buffer, const Theme& theme,
                             ImFont* bold_font, ImFont* italic_font, ImFont* h1_font, ImFont* h2_font) {
    if (!buffer) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.background);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    ImGui::BeginChild(id, ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    bool inside_code_block = false;
    std::string code_block_content;
    std::string code_block_lang;
    int code_block_id = 0; // unique ID counter for code block children

    bool in_table = false;
    int table_cols = 0;

    float lh = ImGui::GetTextLineHeightWithSpacing();

    auto render_inline_markdown = [&](const std::string& text) {
        const char* p   = text.c_str();
        const char* end = p + text.size();
        bool first_in_line = true;
        float wrap_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        auto print_wrapped = [&](const std::string& str) {
            if (str.empty()) return;
            const char* s = str.c_str();
            while (*s) {
                const char* w_end = s;
                while (*w_end && *w_end != ' ' && *w_end != '\n') ++w_end;
                bool is_space = (*w_end == ' ');
                bool is_newline = (*w_end == '\n');

                std::string word(s, w_end - s);
                if (!word.empty()) {
                    float w_width = ImGui::CalcTextSize(word.c_str()).x;
                    if (!first_in_line && ImGui::GetCursorScreenPos().x + w_width > wrap_x) {
                        ImGui::NewLine();
                        first_in_line = true;
                    }
                    if (!first_in_line) ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted(word.c_str());
                    first_in_line = false;
                }

                if (is_space) {
                    float sp_width = ImGui::CalcTextSize(" ").x;
                    if (!first_in_line && ImGui::GetCursorScreenPos().x + sp_width > wrap_x) {
                        ImGui::NewLine();
                        first_in_line = true;
                    } else {
                        if (!first_in_line) ImGui::SameLine(0.0f, 0.0f);
                        ImGui::TextUnformatted(" ");
                        first_in_line = false;
                    }
                    s = w_end + 1;
                } else if (is_newline) {
                    ImGui::NewLine();
                    first_in_line = true;
                    s = w_end + 1;
                } else {
                    s = w_end;
                }
            }
        };

        const char* seg_start = p;
        while (p < end) {
            // Inline code `...`
            if (*p == '`') {
                print_wrapped(std::string(seg_start, p - seg_start));
                const char* code_start = p + 1;
                const char* code_end   = code_start;
                while (code_end < end && *code_end != '`') ++code_end;
                if (code_end < end) {
                    ImGui::PushStyleColor(ImGuiCol_Text, theme.syntax_string);
                    print_wrapped(std::string(code_start, code_end - code_start));
                    ImGui::PopStyleColor();
                    p = code_end + 1;
                } else {
                    p = code_end;
                }
                seg_start = p;
                continue;
            }
            // Links & Images: ![Alt](URL) or [Text](URL)
            bool is_img = (*p == '!' && p + 1 < end && *(p+1) == '[');
            bool is_link = (*p == '[');
            if (is_img || is_link) {
                const char* bracket_start = is_img ? p + 2 : p + 1;
                const char* bracket_end = bracket_start;
                while (bracket_end < end && *bracket_end != ']') ++bracket_end;
                
                if (bracket_end < end && bracket_end + 1 < end && *(bracket_end + 1) == '(') {
                    const char* paren_start = bracket_end + 2;
                    const char* paren_end = paren_start;
                    while (paren_end < end && *paren_end != ')') ++paren_end;
                    
                    if (paren_end < end) {
                        print_wrapped(std::string(seg_start, p - seg_start));
                        std::string label(bracket_start, bracket_end - bracket_start);
                        std::string url(paren_start, paren_end - paren_start);
                        
                        // Special handling for shields.io badges
                        if (is_img && url.substr(0, 29) == "https://img.shields.io/badge/") {
                            std::string badge_data = url.substr(29);
                            size_t first_dash = badge_data.find('-');
                            size_t last_dash = badge_data.rfind('-');
                            
                            auto url_decode = [](const std::string& str) {
                                std::string ret;
                                for (size_t i = 0; i < str.length(); ++i) {
                                    if (str[i] == '%' && i + 2 < str.length()) {
                                        int val;
                                        if (sscanf(str.substr(i + 1, 2).c_str(), "%x", &val) == 1) {
                                            ret += static_cast<char>(val);
                                            i += 2;
                                        } else ret += str[i];
                                    } else if (str[i] == '_') {
                                        ret += ' ';
                                    } else ret += str[i];
                                }
                                return ret;
                            };
                            
                            if (first_dash != std::string::npos && last_dash != std::string::npos && first_dash != last_dash) {
                                std::string b_left = url_decode(badge_data.substr(0, first_dash));
                                std::string b_right = url_decode(badge_data.substr(first_dash + 1, last_dash - first_dash - 1));
                                std::string b_color = badge_data.substr(last_dash + 1);
                                
                                ImVec4 color_right = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
                                if (b_color == "orange") color_right = ImVec4(0.9f, 0.5f, 0.1f, 1.0f);
                                else if (b_color == "blue") color_right = ImVec4(0.1f, 0.5f, 0.9f, 1.0f);
                                else if (b_color == "green") color_right = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
                                else if (b_color == "lightgrey") color_right = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                                
                                ImVec4 color_left = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
                                
                                ImVec2 s_left = ImGui::CalcTextSize(b_left.c_str());
                                ImVec2 s_right = ImGui::CalcTextSize(b_right.c_str());
                                ImVec2 pad(6.0f, 2.0f);
                                
                                float w = s_left.x + s_right.x + pad.x * 4.0f;
                                float h = std::max(s_left.y, s_right.y) + pad.y * 2.0f;
                                
                                if (!first_in_line && ImGui::GetCursorScreenPos().x + w > wrap_x) {
                                    ImGui::NewLine();
                                    first_in_line = true;
                                }
                                if (!first_in_line) ImGui::SameLine(0.0f, 0.0f);
                                
                                ImVec2 p0 = ImGui::GetCursorScreenPos();
                                ImVec2 p_mid = ImVec2(p0.x + s_left.x + pad.x * 2.0f, p0.y + h);
                                ImVec2 p1 = ImVec2(p0.x + w, p0.y + h);
                                
                                ImDrawList* dl = ImGui::GetWindowDrawList();
                                float r = 4.0f;
                                dl->AddRectFilled(p0, ImVec2(p_mid.x, p_mid.y), ImGui::GetColorU32(color_left), r, ImDrawFlags_RoundCornersLeft);
                                dl->AddRectFilled(ImVec2(p_mid.x, p0.y), p1, ImGui::GetColorU32(color_right), r, ImDrawFlags_RoundCornersRight);
                                
                                dl->AddText(ImVec2(p0.x + pad.x, p0.y + pad.y), ImGui::GetColorU32(ImVec4(1,1,1,1)), b_left.c_str());
                                dl->AddText(ImVec2(p_mid.x + pad.x, p0.y + pad.y), ImGui::GetColorU32(ImVec4(1,1,1,1)), b_right.c_str());
                                
                                ImGui::Dummy(ImVec2(w, h));
                                first_in_line = false;
                                
                                if (ImGui::IsItemHovered()) {
                                    ImGui::SetTooltip("%s", url.c_str());
                                }
                                p = paren_end + 1;
                                seg_start = p;
                                continue;
                            }
                        }

                        // Normal image or link
                        ImGui::PushStyleColor(ImGuiCol_Text, is_img ? theme.syntax_string : theme.syntax_keyword);
                        if (is_img) print_wrapped("[Img: " + label + "]");
                        else print_wrapped(label);
                        
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s", url.c_str());
                        }
                        ImGui::PopStyleColor();
                        
                        p = paren_end + 1;
                        seg_start = p;
                        continue;
                    }
                }
            }
            // Bold **...**
            if (*p == '*' && p + 1 < end && *(p+1) == '*') {
                print_wrapped(std::string(seg_start, p - seg_start));
                const char* b_start = p + 2;
                const char* b_end   = b_start;
                while (b_end + 1 < end && !(b_end[0] == '*' && b_end[1] == '*')) ++b_end;
                if (b_end + 1 < end) {
                    if (bold_font) ImGui::PushFont(bold_font);
                    print_wrapped(std::string(b_start, b_end - b_start));
                    if (bold_font) ImGui::PopFont();
                    p = b_end + 2;
                } else {
                    p = b_end;
                }
                seg_start = p;
                continue;
            }
            ++p;
        }
        print_wrapped(std::string(seg_start, p - seg_start));
    };

    for (int i = 0; i < buffer->GetLineCount(); ++i) {
        std::string line = RemoveEmojis(buffer->GetLine(i));
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r')) line.pop_back();

        // Detect ``` fences
        int ts = 0;
        while (ts < static_cast<int>(line.size()) && line[ts] == ' ') ++ts;

        bool is_fence = ts + 2 < static_cast<int>(line.size()) &&
                        line[ts] == '`' && line[ts+1] == '`' && line[ts+2] == '`';

        if (is_fence) {
            if (in_table) { ImGui::EndTable(); ImGui::Spacing(); in_table = false; }
            if (inside_code_block) {
                // Render accumulated code block as a styled text block (no nested BeginChild)
                ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.sidebar_bg);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
                int line_count = static_cast<int>(std::count(code_block_content.begin(), code_block_content.end(), '\n')) + 1;
                float block_h = lh * static_cast<float>(line_count) + 20.0f;
                // Clamp to avoid zero/negative height
                if (block_h < 30.0f) block_h = 30.0f;
                char cb_id[32];
                snprintf(cb_id, sizeof(cb_id), "##code_blk_%d", code_block_id++);
                ImGui::BeginChild(cb_id, ImVec2(-1.0f, block_h), true);
                
                if (!code_block_lang.empty()) {
                    ImVec2 cur_pos = ImGui::GetCursorPos();
                    float lang_w = ImGui::CalcTextSize(code_block_lang.c_str()).x;
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - lang_w - 10.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    ImGui::TextUnformatted(code_block_lang.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SetCursorPos(cur_pos);
                }
                
                ImGui::PushStyleColor(ImGuiCol_Text, theme.syntax_string);
                ImGui::TextUnformatted(code_block_content.c_str());
                ImGui::PopStyleColor();
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                ImGui::Spacing();
                code_block_content.clear();
                inside_code_block = false;
            } else {
                inside_code_block = true;
                code_block_content.clear();
                code_block_lang = "";
                size_t lang_start = ts + 3;
                while (lang_start < line.size() && line[lang_start] == ' ') ++lang_start;
                if (lang_start < line.size()) {
                    code_block_lang = line.substr(lang_start);
                }
            }
            continue;
        }

        if (inside_code_block) {
            if (!code_block_content.empty()) code_block_content += "\n";
            code_block_content += line;
            continue;
        }

        bool is_table_row = (!line.empty() && line.front() == '|' && line.back() == '|');
        if (!is_table_row && in_table) {
            ImGui::EndTable();
            ImGui::Spacing();
            in_table = false;
        }
        if (is_table_row) {
            int cols = 0;
            for (char c : line) if (c == '|') ++cols;
            cols -= 1;
            
            if (cols > 0) {
                if (!in_table) {
                    table_cols = cols;
                    ImGui::BeginTable("##md_table", table_cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame);
                    in_table = true;
                }
                
                if (line.find("---") != std::string::npos) {
                    continue;
                }
                
                ImGui::TableNextRow();
                size_t start = 1;
                for (int col = 0; col < table_cols; ++col) {
                    ImGui::TableSetColumnIndex(col);
                    size_t end = line.find('|', start);
                    if (end == std::string::npos) end = line.size() - 1;
                    
                    std::string cell = line.substr(start, end - start);
                    size_t c_start = cell.find_first_not_of(" \t");
                    size_t c_end = cell.find_last_not_of(" \t");
                    if (c_start != std::string::npos) cell = cell.substr(c_start, c_end - c_start + 1);
                    else cell = "";
                    
                    render_inline_markdown(cell);
                    start = end + 1;
                }
                continue;
            }
        }

        // H1
        if (line.size() >= 2 && line[0] == '#' && line[1] == ' ') {
            if (h1_font) ImGui::PushFont(h1_font);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.foreground);
            render_inline_markdown(std::string(line.c_str() + 2));
            ImGui::PopStyleColor();
            if (h1_font) ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();
            continue;
        }
        // H2
        if (line.size() >= 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ') {
            if (h2_font) ImGui::PushFont(h2_font);
            else if (bold_font) ImGui::PushFont(bold_font);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.syntax_keyword);
            render_inline_markdown(std::string(line.c_str() + 3));
            ImGui::PopStyleColor();
            if (h2_font || bold_font) ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();
            continue;
        }
        // H3
        if (line.size() >= 4 && line[0]=='#' && line[1]=='#' && line[2]=='#' && line[3]==' ') {
            if (bold_font) ImGui::PushFont(bold_font);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.syntax_type);
            render_inline_markdown(std::string(line.c_str() + 4));
            ImGui::PopStyleColor();
            if (bold_font) ImGui::PopFont();
            ImGui::Spacing();
            continue;
        }
        // H4/H5
        if (line.size() >= 5 && line[0]=='#' && line[1]=='#' && line[2]=='#' && line[3]=='#' && line[4]==' ') {
            if (bold_font) ImGui::PushFont(bold_font);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.syntax_function);
            render_inline_markdown(std::string(line.c_str() + 5));
            ImGui::PopStyleColor();
            if (bold_font) ImGui::PopFont();
            ImGui::Spacing();
            continue;
        }

        // Horizontal rule
        if (line == "---" || line == "***" || line == "___") {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            continue;
        }

        // Blockquote
        if (line.size() >= 2 && line[0] == '>' && line[1] == ' ') {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.syntax_comment);
            ImGui::Bullet();
            ImGui::SameLine();
            if (italic_font) ImGui::PushFont(italic_font);
            ImGui::TextWrapped("%s", line.c_str() + 2);
            if (italic_font) ImGui::PopFont();
            ImGui::PopStyleColor();
            continue;
        }

        // List item (-, *, +)
        if (line.size() >= 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ') {
            ImGui::Bullet();
            ImGui::SameLine();
            render_inline_markdown(std::string(line.c_str() + 2));
            continue;
        }

        // Numbered list  "1. "
        if (line.size() >= 3 && std::isdigit(static_cast<unsigned char>(line[0]))) {
            size_t dot = line.find(". ");
            if (dot != std::string::npos && dot < 4) {
                ImGui::Bullet();
                ImGui::SameLine();
                render_inline_markdown(std::string(line.c_str() + dot + 2));
                continue;
            }
        }

        // Empty line → spacing
        if (line.empty()) {
            ImGui::Spacing();
            continue;
        }

        // Regular paragraph — render with basic inline formatting
        render_inline_markdown(line);
    }

    // If a code block was never closed, render remaining content
    if (inside_code_block && !code_block_content.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.syntax_string);
        ImGui::TextUnformatted(code_block_content.c_str());
        ImGui::PopStyleColor();
    }
    
    if (in_table) {
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

}  // namespace luce
