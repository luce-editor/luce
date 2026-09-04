// ============================================================================
// CommandPalette — Implementation.
// ============================================================================

#include "command_palette.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>

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

void CommandPalette::Open(PaletteMode mode) {
    open_               = true;
    mode_               = mode;
    input_buf_[0]       = '\0';
    focus_input_frames_ = 5;
    selected_index_     = 0;

    if (mode == PaletteMode::GoToLine) {
        input_buf_[0] = ':';
        input_buf_[1] = '\0';
    }
}

void CommandPalette::Close() {
    open_ = false;
    focus_input_frames_ = 0;
}

void CommandPalette::SetProjectFiles(const std::vector<std::string>& files) {
    project_files_ = files;
}

/// Render the palette as a centered, floating ImGui window with a search
/// bar and a scrollable list of matching items.
void CommandPalette::Render() {
    if (!open_) return;

    // Center the palette horizontally near the top of the viewport.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float palette_width  = 500.0f;
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

    // Search input.
    ImGui::SetNextItemWidth(-1);
    if (focus_input_frames_ > 0 || ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
        if (focus_input_frames_ > 0) {
            focus_input_frames_--;
        }
    }

    const char* hint = "Type a command...";
    if (mode_ == PaletteMode::Files) hint = "Search files by name...";
    if (mode_ == PaletteMode::GoToLine) hint = "Go to line (:number)...";

    ImGui::InputTextWithHint("##palette_input", hint,
        input_buf_, sizeof(input_buf_),
        ImGuiInputTextFlags_EnterReturnsTrue);

    // Close on Escape.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        Close();
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    std::string query(input_buf_);
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
    }

    if (!visible_indices.empty()) {
        selected_index_ = std::clamp(selected_index_, 0, static_cast<int>(visible_indices.size()) - 1);
    } else {
        selected_index_ = 0;
    }

    // Arrow key navigation.
    bool selection_changed = false;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
        if (!visible_indices.empty()) {
            selected_index_ = std::max(0, selected_index_ - 1);
            selection_changed = true;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
        if (!visible_indices.empty()) {
            selected_index_ = std::min(static_cast<int>(visible_indices.size()) - 1, selected_index_ + 1);
            selection_changed = true;
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !visible_indices.empty()) {
        int i = visible_indices[selected_index_];
        if (mode_ == PaletteMode::Commands) {
            commands_[i].action();
        } else if (mode_ == PaletteMode::Files) {
            if (on_open_file_) on_open_file_(project_files_[i]);
        } else if (mode_ == PaletteMode::GoToLine) {
            std::string num;
            for (char ch : query) {
                if (std::isdigit(static_cast<unsigned char>(ch))) num += ch;
            }
            if (!num.empty() && on_go_to_line_) {
                on_go_to_line_(std::stoi(num) - 1);
            }
        }
        Close();
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
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
    }

    else if (mode_ == PaletteMode::Files) {
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
    }

    else if (mode_ == PaletteMode::GoToLine) {
        std::string num;
        for (char ch : query) {
            if (std::isdigit(static_cast<unsigned char>(ch))) num += ch;
        }
        if (!num.empty()) {
            int line = std::stoi(num) - 1;
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
