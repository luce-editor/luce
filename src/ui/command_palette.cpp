// ============================================================================
// CommandPalette — Implementation.
// ============================================================================

#include "command_palette.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace luce {

CommandPalette::CommandPalette() = default;

void CommandPalette::RegisterCommand(Command cmd) {
    commands_.push_back(std::move(cmd));
}

void CommandPalette::UnregisterCommandsWithPrefix(const std::string& prefix) {
    std::erase_if(commands_, [&](const Command& cmd) {
        return cmd.id.starts_with(prefix);
    });
}

void CommandPalette::SetMode(PaletteMode mode) {
    if (mode_ != mode) {
        mode_ = mode;
        selected_index_ = 0;
        search_results_.clear();
        last_search_query_.clear();
    }
}

void CommandPalette::Open(PaletteMode mode) {
    open_               = true;
    mode_               = mode;
    input_buf_[0]       = '\0';
    focus_input_frames_ = 5;
    selected_index_     = 0;
    search_results_.clear();
    last_search_query_.clear();
}

void CommandPalette::Close() {
    open_ = false;
    focus_input_frames_ = 0;
    search_results_.clear();
    last_search_query_.clear();
}

void CommandPalette::SetProjectFiles(const std::vector<std::string>& files) {
    project_files_ = files;
}

void CommandPalette::PerformProjectSearch(const std::string& query) {
    if (query.empty() || query.size() < 2) {
        search_results_.clear();
        last_search_query_ = query;
        return;
    }

    if (query == last_search_query_) return;
    last_search_query_ = query;
    search_results_.clear();

    std::string lower_query = query;
    std::ranges::transform(lower_query, lower_query.begin(), ::tolower);

    for (const auto& rel_file : project_files_) {
        // Skip binary and media files based on extension
        auto dot_pos = rel_file.find_last_of('.');
        if (dot_pos != std::string::npos) {
            std::string ext = rel_file.substr(dot_pos);
            std::ranges::transform(ext, ext.begin(), ::tolower);
            if (ext == ".exe" || ext == ".dll" || ext == ".lib" || ext == ".obj" ||
                ext == ".pdb" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                ext == ".gif" || ext == ".ico" || ext == ".zip" || ext == ".tar" ||
                ext == ".gz"  || ext == ".ttf" || ext == ".otf" || ext == ".bin" ||
                ext == ".wasm"|| ext == ".a"   || ext == ".so"  || ext == ".dylib" ||
                ext == ".bmp" || ext == ".webp") {
                continue;
            }
        }

        std::string full_path = project_root_.empty() ? rel_file : (project_root_ + "/" + rel_file);
        std::ifstream in(full_path, std::ios::binary);
        if (!in.is_open()) continue;

        std::string line;
        int line_num = 1;
        while (std::getline(in, line)) {
            // Strip any trailing carriage return (\r\n)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Skip overly long or minified lines
            if (line.size() > 500) {
                ++line_num;
                continue;
            }

            std::string lower_line = line;
            std::ranges::transform(lower_line, lower_line.begin(), ::tolower);

            auto pos = lower_line.find(lower_query);
            if (pos != std::string::npos) {
                size_t first_non_space = line.find_first_not_of(" \t");
                std::string trimmed = (first_non_space != std::string::npos) ? line.substr(first_non_space) : line;
                if (trimmed.size() > 140) {
                    trimmed = trimmed.substr(0, 140) + "...";
                }

                SearchResult sr;
                sr.file_path = rel_file;
                sr.full_path = full_path;
                sr.line_number = line_num;
                sr.col_number = static_cast<int>(pos) + 1;
                sr.line_content = std::move(trimmed);
                search_results_.push_back(std::move(sr));

                if (search_results_.size() >= 100) break;
            }
            ++line_num;
        }

        if (search_results_.size() >= 100) break;
    }
}

/// Render the palette as a centered, floating ImGui window with a search
/// bar and a scrollable list of matching items.
void CommandPalette::Render() {
    if (!open_) return;

    // Center the palette horizontally near the top of the viewport.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float palette_width  = (mode_ == PaletteMode::ProjectSearch) ? 620.0f : 520.0f;
    float palette_x      = viewport->Pos.x + (viewport->Size.x - palette_width) * 0.5f;
    float palette_y      = viewport->Pos.y + 80.0f;

    ImGui::SetNextWindowPos(ImVec2(palette_x, palette_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(palette_width, 0), ImGuiCond_Always);

    if (focus_input_frames_ > 0) {
        ImGui::SetNextWindowFocus();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));

    ImGui::Begin("##command_palette", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_AlwaysAutoResize);

    // Dynamic mode switching / prefix cleanup if input_buf_ starts with a prefix character
    if (input_buf_[0] == '>') {
        SetMode(PaletteMode::Commands);
        int shift = (input_buf_[1] == ' ') ? 2 : 1;
        std::memmove(input_buf_, input_buf_ + shift, std::strlen(input_buf_ + shift) + 1);
    } else if (input_buf_[0] == ':') {
        SetMode(PaletteMode::GoToLine);
        int shift = (input_buf_[1] == ' ') ? 2 : 1;
        std::memmove(input_buf_, input_buf_ + shift, std::strlen(input_buf_ + shift) + 1);
    } else if (input_buf_[0] == '?') {
        SetMode(PaletteMode::ProjectSearch);
        int shift = (input_buf_[1] == ' ') ? 2 : 1;
        std::memmove(input_buf_, input_buf_ + shift, std::strlen(input_buf_ + shift) + 1);
    } else if (mode_ != PaletteMode::Files && input_buf_[0] == '\0' && ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        SetMode(PaletteMode::Files);
    }

    const char* prompt = nullptr;
    const char* hint = "Type a command...";
    if (mode_ == PaletteMode::Commands) {
        prompt = ">";
        hint = "Type a command...";
    } else if (mode_ == PaletteMode::Files) {
        prompt = "";
        hint = "Search files by name...";
    } else if (mode_ == PaletteMode::GoToLine) {
        prompt = ":";
        hint = "Go to line number...";
    } else if (mode_ == PaletteMode::ProjectSearch) {
        prompt = "?";
        hint = "Search text in project files...";
    }

    bool has_prompt = (prompt && prompt[0] != '\0');
    ImVec2 input_screen_pos = ImGui::GetCursorScreenPos();

    // Search input with left padding for prompt symbol
    if (has_prompt) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(26.0f, 7.0f));
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 7.0f));
    }

    ImGui::SetNextItemWidth(-1);
    if (focus_input_frames_ > 0 || ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
        if (focus_input_frames_ > 0) {
            focus_input_frames_--;
        }
    }

    auto input_callback = [](ImGuiInputTextCallbackData* data) -> int {
        auto* palette = static_cast<CommandPalette*>(data->UserData);
        if (!palette) return 0;

        if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
            bool at_start = (data->CursorPos == 0) ||
                            (data->HasSelection() && data->SelectionStart == 0);
            if (at_start) {
                if (data->EventChar == '>') {
                    if (data->HasSelection()) {
                        data->DeleteChars(0, data->BufTextLen);
                    }
                    palette->SetMode(PaletteMode::Commands);
                    return 1; // Drop '>' so it is not duplicated into input buffer
                } else if (data->EventChar == ':') {
                    if (data->HasSelection()) {
                        data->DeleteChars(0, data->BufTextLen);
                    }
                    palette->SetMode(PaletteMode::GoToLine);
                    return 1; // Drop ':'
                } else if (data->EventChar == '?') {
                    if (data->HasSelection()) {
                        data->DeleteChars(0, data->BufTextLen);
                    }
                    palette->SetMode(PaletteMode::ProjectSearch);
                    return 1; // Drop '?'
                } else if (data->EventChar == ' ' && data->BufTextLen == 0) {
                    return 1; // Discard leading space after mode prefix
                }
            }
        }
        return 0;
    };

    ImGui::InputTextWithHint("##palette_input", hint,
        input_buf_, sizeof(input_buf_),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter,
        input_callback, this);

    if (ImGui::IsItemActive()) {
        focus_input_frames_ = 0;
    }

    // Draw the prompt symbol inside the input box on the left
    if (has_prompt) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 prompt_col = (mode_ == PaletteMode::Commands)
            ? IM_COL32(110, 180, 255, 255)
            : (mode_ == PaletteMode::ProjectSearch)
                ? IM_COL32(100, 220, 180, 255)
                : IM_COL32(210, 210, 210, 255);
        dl->AddText(ImVec2(input_screen_pos.x + 9.0f, input_screen_pos.y + 7.0f), prompt_col, prompt);
    }
    ImGui::PopStyleVar();

    // Close on Escape.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        Close();
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    // Ensure leading prefix is never present in query
    if (mode_ == PaletteMode::Commands && input_buf_[0] == '>') {
        int shift = (input_buf_[1] == ' ') ? 2 : 1;
        std::memmove(input_buf_, input_buf_ + shift, std::strlen(input_buf_ + shift) + 1);
    } else if (mode_ == PaletteMode::GoToLine && input_buf_[0] == ':') {
        int shift = (input_buf_[1] == ' ') ? 2 : 1;
        std::memmove(input_buf_, input_buf_ + shift, std::strlen(input_buf_ + shift) + 1);
    } else if (mode_ == PaletteMode::ProjectSearch && input_buf_[0] == '?') {
        int shift = (input_buf_[1] == ' ') ? 2 : 1;
        std::memmove(input_buf_, input_buf_ + shift, std::strlen(input_buf_ + shift) + 1);
    }

    std::string query(input_buf_);
    size_t first_non_space = query.find_first_not_of(" \t");
    if (first_non_space != std::string::npos) {
        query = query.substr(first_non_space);
    } else if (!query.empty()) {
        query.clear();
    }
    std::vector<int> visible_indices;

    if (mode_ == PaletteMode::Commands) {
        for (int i = 0; i < static_cast<int>(commands_.size()); ++i) {
            if (!query.empty() && !FuzzyMatch(commands_[i].display_name, query))
                continue;
            visible_indices.push_back(i);
        }
    } else if (mode_ == PaletteMode::Files) {
        for (int i = 0; i < static_cast<int>(project_files_.size()); ++i) {
            if (!query.empty() && !FuzzyMatch(project_files_[i], query))
                continue;
            visible_indices.push_back(i);
        }
    } else if (mode_ == PaletteMode::ProjectSearch) {
        PerformProjectSearch(query);
    }

    int total_results = (mode_ == PaletteMode::ProjectSearch)
        ? static_cast<int>(search_results_.size())
        : static_cast<int>(visible_indices.size());

    if (total_results > 0) {
        selected_index_ = std::clamp(selected_index_, 0, total_results - 1);
    } else {
        selected_index_ = 0;
    }

    // Arrow key navigation.
    bool selection_changed = false;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
        if (total_results > 0) {
            selected_index_ = std::max(0, selected_index_ - 1);
            selection_changed = true;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
        if (total_results > 0) {
            selected_index_ = std::min(total_results - 1, selected_index_ + 1);
            selection_changed = true;
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (mode_ == PaletteMode::Commands && !visible_indices.empty()) {
            int i = visible_indices[selected_index_];
            commands_[i].action();
            Close();
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        } else if (mode_ == PaletteMode::Files && !visible_indices.empty()) {
            int i = visible_indices[selected_index_];
            if (on_open_file_) on_open_file_(project_files_[i]);
            Close();
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        } else if (mode_ == PaletteMode::ProjectSearch && !search_results_.empty()) {
            const auto& sr = search_results_[selected_index_];
            if (on_open_file_at_line_) {
                on_open_file_at_line_(sr.full_path, sr.line_number, sr.col_number);
            }
            Close();
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        } else if (mode_ == PaletteMode::GoToLine) {
            std::string num;
            for (char ch : query) {
                if (std::isdigit(static_cast<unsigned char>(ch))) num += ch;
            }
            if (!num.empty() && on_go_to_line_) {
                on_go_to_line_(std::stoi(num) - 1);
            }
            Close();
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        }
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##palette_results", ImVec2(0, 300), false, ImGuiWindowFlags_NoNavFocus);

    if (mode_ == PaletteMode::Commands) {
        for (int visible = 0; visible < static_cast<int>(visible_indices.size()); ++visible) {
            int i = visible_indices[visible];
            bool selected = (visible == selected_index_);
            std::string label = commands_[i].display_name;
            if (!commands_[i].shortcut.empty()) {
                label += "  [" + commands_[i].shortcut + "]";
            }

            if (selected && selection_changed) {
                ImGui::SetScrollHereY(0.5f);
            }

            if (ImGui::Selectable(label.c_str(), selected)) {
                commands_[i].action();
                Close();
            }
        }
    } else if (mode_ == PaletteMode::Files) {
        for (int visible = 0; visible < static_cast<int>(visible_indices.size()); ++visible) {
            int i = visible_indices[visible];
            bool selected = (visible == selected_index_);
            if (selected && selection_changed) {
                ImGui::SetScrollHereY(0.5f);
            }
            if (ImGui::Selectable(project_files_[i].c_str(), selected)) {
                if (on_open_file_) on_open_file_(project_files_[i]);
                Close();
            }
        }
    } else if (mode_ == PaletteMode::ProjectSearch) {
        if (query.size() < 2) {
            ImGui::Spacing();
            ImGui::TextDisabled("  Type at least 2 characters to search across project files...");
        } else if (search_results_.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("  No matching text found in project files.");
        } else {
            for (int i = 0; i < static_cast<int>(search_results_.size()); ++i) {
                const auto& sr = search_results_[i];
                bool selected = (i == selected_index_);
                if (selected && selection_changed) {
                    ImGui::SetScrollHereY(0.5f);
                }

                ImGui::PushID(i);

                auto slash = sr.file_path.find_last_of("/\\");
                std::string fname = (slash != std::string::npos) ? sr.file_path.substr(slash + 1) : sr.file_path;
                std::string fdir  = (slash != std::string::npos) ? sr.file_path.substr(0, slash) : "";
                std::string title = fname + ":" + std::to_string(sr.line_number);

                ImVec2 item_size(0, ImGui::GetTextLineHeightWithSpacing() * 2.1f);
                if (ImGui::Selectable("##search_item", selected, 0, item_size)) {
                    if (on_open_file_at_line_) {
                        on_open_file_at_line_(sr.full_path, sr.line_number, sr.col_number);
                    }
                    Close();
                    ImGui::PopID();
                    break;
                }

                ImVec2 rect_min = ImGui::GetItemRectMin();
                ImDrawList* dl = ImGui::GetWindowDrawList();

                // Line 1: Title (filename:line)
                dl->AddText(ImVec2(rect_min.x + 6.0f, rect_min.y + 2.0f),
                            IM_COL32(245, 245, 245, 255), title.c_str());

                // Directory in dimmed gray
                if (!fdir.empty()) {
                    float title_w = ImGui::CalcTextSize(title.c_str()).x;
                    dl->AddText(ImVec2(rect_min.x + 6.0f + title_w + 10.0f, rect_min.y + 2.0f),
                                IM_COL32(130, 130, 140, 255), fdir.c_str());
                }

                // Line 2: Code preview snippet
                dl->AddText(ImVec2(rect_min.x + 14.0f, rect_min.y + ImGui::GetTextLineHeightWithSpacing() + 1.0f),
                            IM_COL32(170, 205, 240, 230), sr.line_content.c_str());

                ImGui::PopID();
            }
        }
    } else if (mode_ == PaletteMode::GoToLine) {
        std::string num;
        for (char ch : query) {
            if (std::isdigit(static_cast<unsigned char>(ch))) num += ch;
        }
        if (!num.empty()) {
            std::string info = "Go to line " + num;
            ImGui::TextUnformatted(info.c_str());
        } else {
            ImGui::TextDisabled("Enter a line number...");
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

/// Simple subsequence fuzzy matcher: all characters of `pattern` must
/// appear in `text` in order (case-insensitive).
bool CommandPalette::FuzzyMatch(const std::string& text,
                                 const std::string& pattern) const {
    size_t pi = 0;
    for (size_t ti = 0; ti < text.size() && pi < pattern.size(); ++ti) {
        if (std::tolower(static_cast<unsigned char>(text[ti])) ==
            std::tolower(static_cast<unsigned char>(pattern[pi]))) {
            ++pi;
        }
    }
    return pi == pattern.size();
}

}  // namespace luce
