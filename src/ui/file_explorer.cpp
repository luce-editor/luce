// ============================================================================
// FileExplorer — Implementation.
//
// Recursively scans the filesystem and presents a collapsible tree using
// ImGui::TreeNode.  Directories are sorted before files; both are sorted
// alphabetically.
// ============================================================================

#include "file_explorer.h"
#include "icon_manager.h"
#include "platform.h"
#include "../editor/git_manager.h"

#include "imgui.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace luce {

FileExplorer::FileExplorer() = default;

void FileExplorer::SetRoot(const std::string& path) {
    root_ = path;
}

void FileExplorer::Render() {
    if (root_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "> No Folder Opened");
        ImGui::Spacing();
        ImGui::TextWrapped("You have not yet added a folder to the workspace.");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.05f, 0.45f, 0.75f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.03f, 0.35f, 0.65f, 1.0f));
        
        if (ImGui::Button("Open Folder", ImVec2(-1, 30))) {
            if (on_open_folder_) on_open_folder_();
        }
        
        ImGui::Spacing();
        ImGui::TextWrapped("You can clone a repository locally.");
        ImGui::Spacing();

        if (ImGui::Button("Clone Repository", ImVec2(-1, 30))) {
            // Placeholder for clone
        }

        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::TextDisabled("To learn more about how to use Git and");
        ImGui::TextDisabled("source control in Luce read our docs.");
        return;
    }

    // Filter input.
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Filter files...", filter_buf_, sizeof(filter_buf_));

    ImGui::Separator();

    ImGui::BeginChild("##file_tree", ImVec2(0, 0), false);
    
    // Display root folder name header (VS Code style) with Refresh button
    std::string root_name = fs::path(root_).filename().string();
    if (root_name.empty()) root_name = root_;
    std::string root_upper = root_name;
    std::ranges::transform(root_upper, root_upper.begin(), ::toupper);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::TextUnformatted(root_upper.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(ImGui::GetWindowWidth() - 28.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
    if (ImGui::Button("##refresh_btn", ImVec2(20, 18))) {
        // Refresh trigger
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Refresh Explorer");
    }

    // Draw refresh icon symbol inside button
    ImVec2 btn_min = ImGui::GetItemRectMin();
    ImVec2 btn_max = ImGui::GetItemRectMax();
    ImDrawList* bg_dl = ImGui::GetWindowDrawList();
    ImVec2 icon_center((btn_min.x + btn_max.x) * 0.5f, (btn_min.y + btn_max.y) * 0.5f);
    bg_dl->PathArcTo(icon_center, 5.0f, 0.5f, 5.8f, 16);
    bg_dl->PathStroke(IM_COL32(200, 200, 200, 255), 0, 1.5f);
    // Draw arrow tip on the arc
    bg_dl->AddTriangleFilled(
        ImVec2(icon_center.x + 4.0f, icon_center.y - 4.5f),
        ImVec2(icon_center.x + 7.5f, icon_center.y - 1.0f),
        ImVec2(icon_center.x + 2.0f, icon_center.y - 1.0f),
        IM_COL32(200, 200, 200, 255)
    );

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::Spacing();

    // Context menu for empty workspace background in explorer
    if (ImGui::BeginPopupContextWindow("##explorer_bg_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("New File...")) {
            target_dir_ = root_;
            action_buf_[0] = '\0';
            show_new_file_modal_ = true;
        }
        if (ImGui::MenuItem("New Folder...")) {
            target_dir_ = root_;
            action_buf_[0] = '\0';
            show_new_folder_modal_ = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reveal in File Explorer")) {
            platform::OpenInFileExplorer(root_);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Remove Folder from Workspace")) {
            if (on_remove_folder_) on_remove_folder_();
        }
        ImGui::EndPopup();
    }

    RenderDirectory(root_);
    ImGui::EndChild();

    RenderModals();
}

void FileExplorer::RenderModals() {
    float modal_width = 380.0f;

    if (show_new_file_modal_) {
        ImGui::OpenPopup("New File##modal");
        show_new_file_modal_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(modal_width, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("New File##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file name (in %s):", target_dir_.c_str());
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newfilename", action_buf_, sizeof(action_buf_));
        ImGui::Spacing();

        float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Create", ImVec2(btn_w, 0))) {
            std::string name(action_buf_);
            if (!name.empty()) {
                std::string full = target_dir_ + "/" + name;
                std::ofstream f(full);
                f.close();
                if (on_open_file_) on_open_file_(full);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_new_folder_modal_) {
        ImGui::OpenPopup("New Folder##modal");
        show_new_folder_modal_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(modal_width, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("New Folder##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter folder name (in %s):", target_dir_.c_str());
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newfoldername", action_buf_, sizeof(action_buf_));
        ImGui::Spacing();

        float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Create", ImVec2(btn_w, 0))) {
            std::string name(action_buf_);
            if (!name.empty()) {
                fs::create_directories(target_dir_ + "/" + name);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_rename_modal_) {
        ImGui::OpenPopup("Rename##modal");
        show_rename_modal_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(modal_width, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Rename##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new name:");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##renamename", action_buf_, sizeof(action_buf_));
        ImGui::Spacing();

        float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Rename", ImVec2(btn_w, 0))) {
            std::string new_name(action_buf_);
            if (!new_name.empty()) {
                fs::path p(target_path_);
                fs::path parent = p.parent_path();
                std::error_code ec;
                fs::rename(target_path_, parent / new_name, ec);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_delete_modal_) {
        ImGui::OpenPopup("Delete##modal");
        show_delete_modal_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(modal_width, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Delete##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you want to delete:\n%s?", target_path_.c_str());
        ImGui::Spacing();

        float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Delete", ImVec2(btn_w, 0))) {
            std::error_code ec;
            fs::remove_all(target_path_, ec);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/// Render one directory level as ImGui tree nodes.  Directories come first,
/// then files, both alphabetically sorted.
void FileExplorer::RenderDirectory(const std::string& path) {
    std::vector<fs::directory_entry> dirs, files;

    std::error_code ec;
    for (auto& entry : fs::directory_iterator(path, ec)) {
        if (ec) break;
        std::string name = entry.path().filename().string();

        // Skip hidden files and common noisy directories.
        if (!name.empty() && name[0] == '.') continue;
        if (name == "node_modules" || name == "__pycache__" ||
            name == "build" || name == "target" || name == ".git")
            continue;

        if (entry.is_directory()) dirs.push_back(entry);
        else                      files.push_back(entry);
    }

    auto cmp = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename().string() < b.path().filename().string();
    };
    std::ranges::sort(dirs, cmp);
    std::ranges::sort(files, cmp);

    std::string filter(filter_buf_);

    // Directories.
    for (auto& dir : dirs) {
        std::string name = dir.path().filename().string();
        std::string dir_path = dir.path().string();
        std::ranges::replace(dir_path, '\\', '/');

        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                        ImGuiTreeNodeFlags_SpanAvailWidth;
        
        bool open = ImGui::TreeNodeEx((name + "##dir").c_str(), node_flags, "     %s", name.c_str());

        // Draw real SVG folder texture icon
        ImTextureID folder_tex = IconManager::Instance().GetFolderIcon(open);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 item_min = ImGui::GetItemRectMin();
        float font_size = ImGui::GetFontSize();
        float icon_sz = font_size + 2.0f;
        ImVec2 icon_min(item_min.x + 18.0f, item_min.y + 1.0f);
        ImVec2 icon_max(icon_min.x + icon_sz, icon_min.y + icon_sz);

        if (folder_tex) {
            dl->AddImage(folder_tex, icon_min, icon_max);
        } else {
            dl->AddRectFilled(icon_min, icon_max, IM_COL32(220, 175, 80, 255), 2.0f);
        }

        // Context menu for directories.
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("New File...")) {
                target_dir_ = dir_path;
                action_buf_[0] = '\0';
                show_new_file_modal_ = true;
            }
            if (ImGui::MenuItem("New Folder...")) {
                target_dir_ = dir_path;
                action_buf_[0] = '\0';
                show_new_folder_modal_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reveal in File Explorer")) {
                platform::OpenInFileExplorer(dir_path);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rename...")) {
                target_path_ = dir_path;
                std::snprintf(action_buf_, sizeof(action_buf_), "%s", name.c_str());
                show_rename_modal_ = true;
            }
            if (ImGui::MenuItem("Delete")) {
                target_path_ = dir_path;
                show_delete_modal_ = true;
            }
            ImGui::EndPopup();
        }

        if (open) {
            RenderDirectory(dir_path);
            ImGui::TreePop();
        }
    }

    // Files.
    for (auto& file : files) {
        std::string name = file.path().filename().string();
        std::string file_path = file.path().string();
        std::ranges::replace(file_path, '\\', '/');

        // Apply filter.
        if (!filter.empty()) {
            std::string lower_name = name;
            std::string lower_filter = filter;
            std::ranges::transform(lower_name, lower_name.begin(), ::tolower);
            std::ranges::transform(lower_filter, lower_filter.begin(), ::tolower);
            if (lower_name.find(lower_filter) == std::string::npos)
                continue;
        }

        ImGuiTreeNodeFlags leaf_flags = ImGuiTreeNodeFlags_Leaf |
                                        ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                        ImGuiTreeNodeFlags_SpanAvailWidth;

        char git_code = GitManager::Instance().GetFileStatusCode(file_path);
        bool push_col = false;
        if (git_code == 'M') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.89f, 0.75f, 0.55f, 1.0f)); // Amber/yellow for modified
            push_col = true;
        } else if (git_code == 'U') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.79f, 0.57f, 1.0f)); // Green for untracked
            push_col = true;
        } else if (git_code == 'D') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.35f, 1.0f)); // Red for deleted
            push_col = true;
        }

        ImGui::TreeNodeEx((name + "##file").c_str(), leaf_flags, "     %s", name.c_str());

        if (push_col) {
            ImGui::PopStyleColor();
        }

        // Draw real SVG file icon texture from icons/ directory!
        ImTextureID file_tex = IconManager::Instance().GetIconForFile(name);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 item_min = ImGui::GetItemRectMin();
        float font_size = ImGui::GetFontSize();
        float icon_sz = font_size + 2.0f;
        ImVec2 icon_min(item_min.x + 18.0f, item_min.y + 1.0f);
        ImVec2 icon_max(icon_min.x + icon_sz, icon_min.y + icon_sz);

        if (file_tex) {
            dl->AddImage(file_tex, icon_min, icon_max);
        } else {
            dl->AddRectFilled(icon_min, icon_max, IM_COL32(140, 140, 140, 255), 2.0f);
        }

        // Draw Git status indicator badge ('M', 'U', 'D') on the right
        if (git_code != '\0') {
            ImVec2 item_max = ImGui::GetItemRectMax();
            ImU32 badge_color = (git_code == 'M') ? IM_COL32(228, 192, 140, 255) :
                                (git_code == 'U') ? IM_COL32(115, 201, 145, 255) :
                                IM_COL32(240, 90, 90, 255);
            char badge_str[2] = { git_code, '\0' };
            dl->AddText(ImVec2(item_max.x - 20.0f, item_min.y + 1.0f), badge_color, badge_str);
        }

        // Double-click opens the file in the editor.
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (on_open_file_) on_open_file_(file_path);
        }

        // Context menu for files.
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Open")) {
                if (on_open_file_) on_open_file_(file_path);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reveal in File Explorer")) {
                platform::OpenInFileExplorer(file_path);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rename...")) {
                target_path_ = file_path;
                std::snprintf(action_buf_, sizeof(action_buf_), "%s", name.c_str());
                show_rename_modal_ = true;
            }
            if (ImGui::MenuItem("Delete")) {
                target_path_ = file_path;
                show_delete_modal_ = true;
            }
            ImGui::EndPopup();
        }
    }
}

}  // namespace luce
