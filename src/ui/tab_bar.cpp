// ============================================================================
// TabBar — Implementation.
// ============================================================================

#include "tab_bar.h"
#include "platform.h"
#include "../editor/git_manager.h"
#include "../editor/diagnostic_runner.h"
#include "../editor/symbol_index.h"
#include "icon_manager.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <filesystem>

#if defined(_WIN32)
#   define NOMINMAX
#   include <windows.h>
#   include <GL/gl.h>
#else
#   include <GL/gl.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

namespace luce {

TabBar::TabBar() = default;

/// Open a file.  If the file is already open in a tab, switch to it.
/// Otherwise create a new tab, load the file, and detect the language.
void TabBar::OpenFile(const std::string& path, const Theme* theme) {
    std::string norm_target = path;
    std::ranges::replace(norm_target, '\\', '/');

    // Check if already open.
    std::error_code ec;
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        std::string norm_tab = tabs_[i]->filepath;
        std::ranges::replace(norm_tab, '\\', '/');
        if (norm_tab == norm_target) {
            if (split_view_ && focused_pane_ == 1) {
                split_active_ = i;
            } else {
                active_ = i;
                tab_to_select_ = i;
            }
            return;
        }
#ifdef _WIN32
        if (!norm_tab.empty() && !norm_target.empty() && _stricmp(norm_tab.c_str(), norm_target.c_str()) == 0) {
            if (split_view_ && focused_pane_ == 1) {
                split_active_ = i;
            } else {
                active_ = i;
                tab_to_select_ = i;
            }
            return;
        }
#endif
        if (!norm_tab.empty() && !norm_target.empty()) {
            if (fs::exists(norm_tab, ec) && fs::exists(norm_target, ec) && fs::equivalent(norm_tab, norm_target, ec)) {
                if (split_view_ && focused_pane_ == 1) {
                    split_active_ = i;
                } else {
                    active_ = i;
                    tab_to_select_ = i;
                }
                return;
            }
        }
    }

    auto tab         = std::make_unique<Tab>();
    tab->filepath    = path;
    tab->title       = platform::GetFilename(path);
    tab->buffer      = std::make_unique<TextBuffer>();
    tab->highlighter = std::make_unique<SyntaxHighlighter>();

    std::string ext = platform::GetExtension(path);
    std::string ext_lower = ext;
    std::ranges::transform(ext_lower, ext_lower.begin(), ::tolower);

    if (ext_lower == ".pdf") {
        platform::OpenInFileExplorer(path);
        return;
    }

    if (ext_lower == ".png" || ext_lower == ".jpg" || ext_lower == ".jpeg") {
        tab->is_image = true;
        int channels = 0;
        unsigned char* data = stbi_load(path.c_str(), &tab->image_width, &tab->image_height, &channels, 4);
        if (data) {
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tab->image_width, tab->image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
            tab->image_texture = tex;
        } else {
            tab->is_image = false; // Failed to load, fallback to text?
        }
    }

    if (!tab->is_image) {
        tab->buffer->LoadFromFile(path);

        // Wire up the change callback so edits invalidate syntax cache and refresh symbol index.
        SyntaxHighlighter* hl = tab->highlighter.get();
        TextBuffer* tb = tab->buffer.get();
        std::string tab_path = path;
        SymbolIndex* s_idx = symbol_index_;
        auto on_txt_chg = on_text_changed_;
        tab->buffer->SetChangeCallback([hl, tb, tab_path, s_idx, on_txt_chg](int line, int count) {
            hl->InvalidateLines(line, count);
            if (s_idx && !tab_path.empty()) {
                s_idx->IndexContent(tab_path, tb->GetText());
            }
            if (on_txt_chg) on_txt_chg(line, count);
        });

        // Auto-detect language from extension or full filename (e.g. CMakeLists.txt)
        if (!tab->highlighter->SetLanguageByExtension(ext)) {
            tab->highlighter->SetLanguageByExtension(tab->title);
        }

        tab->editor.SetBuffer(tab->buffer.get());
        tab->editor.SetHighlighter(tab->highlighter.get());
        tab->editor.SetTheme(theme);
        tab->editor.SetFilePath(path);
        tab->editor.show_minimap = show_minimap_;
        tab->editor.SetSymbolIndex(symbol_index_);
        tab->editor.SetOnGoToDefinition(on_goto_definition_);
        tab->editor.SetCompletionProvider(completion_provider_);

        tab->split_editor.SetBuffer(tab->buffer.get());
        tab->split_editor.SetHighlighter(tab->highlighter.get());
        tab->split_editor.SetTheme(theme);
        tab->split_editor.SetFilePath(path);
        tab->split_editor.show_minimap = show_minimap_;
        tab->split_editor.SetSymbolIndex(symbol_index_);
        tab->split_editor.SetOnGoToDefinition(on_goto_definition_);
        tab->split_editor.SetCompletionProvider(completion_provider_);

        // Index the opened file and any sibling source files in the same directory
        if (symbol_index_) {
            symbol_index_->IndexFile(path);
            std::string parent_dir = platform::GetDirectory(path);
            if (!parent_dir.empty()) {
                std::error_code ec;
                for (const auto& entry : fs::directory_iterator(parent_dir, ec)) {
                    if (!entry.is_regular_file(ec)) continue;
                    auto ext = entry.path().extension().string();
                    std::ranges::transform(ext, ext.begin(), ::tolower);
                    if (ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".inl") {
                        symbol_index_->IndexFile(entry.path().generic_string());
                    }
                }
                symbol_index_->IndexDirectoryAsync(parent_dir);
            }
        }

        // Run background diagnostics on open
        DiagnosticRunner::Instance().CheckFile(path);
    }

    tabs_.push_back(std::move(tab));
    int new_idx = static_cast<int>(tabs_.size()) - 1;
    if (split_view_ && focused_pane_ == 1) {
        split_active_ = new_idx;
    } else {
        active_ = new_idx;
        tab_to_select_ = active_;
    }
    if (on_file_opened_ && !path.empty()) {
        on_file_opened_(path);
    }
}

void TabBar::NewFile(const Theme* theme) {
    auto tab         = std::make_unique<Tab>();
    tab->title       = "Untitled";
    tab->buffer      = std::make_unique<TextBuffer>();
    tab->highlighter = std::make_unique<SyntaxHighlighter>();

    SyntaxHighlighter* hl = tab->highlighter.get();
    tab->buffer->SetChangeCallback([hl](int line, int count) {
        hl->InvalidateLines(line, count);
    });

    tab->editor.SetBuffer(tab->buffer.get());
    tab->editor.SetHighlighter(tab->highlighter.get());
    tab->editor.SetTheme(theme);
    tab->editor.show_minimap = show_minimap_;
    tab->editor.SetSymbolIndex(symbol_index_);
    tab->editor.SetOnGoToDefinition(on_goto_definition_);
    tab->editor.SetCompletionProvider(completion_provider_);

    tab->split_editor.SetBuffer(tab->buffer.get());
    tab->split_editor.SetHighlighter(tab->highlighter.get());
    tab->split_editor.SetTheme(theme);
    tab->split_editor.show_minimap = show_minimap_;
    tab->split_editor.SetSymbolIndex(symbol_index_);
    tab->split_editor.SetOnGoToDefinition(on_goto_definition_);
    tab->split_editor.SetCompletionProvider(completion_provider_);

    tabs_.push_back(std::move(tab));
    active_ = static_cast<int>(tabs_.size()) - 1;
    tab_to_select_ = active_;
}

bool TabBar::SaveActive() {
    auto* tab = ActiveTab();
    if (!tab) return false;

    if (tab->filepath.empty()) {
        std::string path = platform::SaveFileDialog();
        if (path.empty()) return false;
        return SaveActiveAs(path);
    }

    if (on_before_save_ && !tab->filepath.empty()) {
        on_before_save_(tab->filepath);
    }

    bool ok = tab->buffer->SaveToFile(tab->filepath);
    if (ok) {
        tab->buffer->ClearDirty();
        GitManager::Instance().RefreshAsync();
        if (symbol_index_) {
            symbol_index_->IndexContent(tab->filepath, tab->buffer->GetText());
        }
        DiagnosticRunner::Instance().CheckFile(tab->filepath);
        if (on_after_save_) {
            on_after_save_(tab->filepath);
        }
    }
    return ok;
}

bool TabBar::SaveActiveAs(const std::string& path) {
    auto* tab = ActiveTab();
    if (!tab) return false;

    if (on_before_save_) {
        on_before_save_(path);
    }

    bool ok = tab->buffer->SaveToFile(path);
    if (ok) {
        tab->filepath = path;
        tab->title    = platform::GetFilename(path);
        tab->editor.SetFilePath(path);
        tab->split_editor.SetFilePath(path);
        tab->buffer->ClearDirty();
        GitManager::Instance().RefreshAsync();

        if (symbol_index_) {
            symbol_index_->IndexContent(path, tab->buffer->GetText());
            std::string parent_dir = platform::GetDirectory(path);
            if (!parent_dir.empty()) {
                symbol_index_->IndexDirectoryAsync(parent_dir);
            }
        }
        DiagnosticRunner::Instance().CheckFile(tab->filepath);

        // Re-detect language.
        std::string ext = platform::GetExtension(path);
        tab->highlighter->SetLanguageByExtension(ext);

        if (on_after_save_) {
            on_after_save_(path);
        }
    }
    return ok;
}

bool TabBar::CloseTab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return true;
    
    if (tabs_[index]->is_image && tabs_[index]->image_texture) {
        GLuint tex = tabs_[index]->image_texture;
        glDeleteTextures(1, &tex);
    }
    
    tabs_.erase(tabs_.begin() + index);
    if (active_ >= static_cast<int>(tabs_.size())) {
        active_ = static_cast<int>(tabs_.size()) - 1;
    }
    tab_to_select_ = active_;

    if (split_active_ == index) {
        split_active_ = -1;
        if (tabs_.size() > 1) {
            split_active_ = (active_ + 1) % static_cast<int>(tabs_.size());
        } else {
            split_view_ = false;
        }
    } else if (split_active_ > index) {
        split_active_--;
    }

    return true;
}

bool TabBar::ReloadTab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return false;
    auto& tab = tabs_[index];
    if (tab->filepath.empty() || tab->is_image) return false;

    if (!fs::exists(tab->filepath)) {
        if (!tab->buffer->IsDirty()) {
            CloseTab(index);
            return true;
        }
        return false;
    }

    if (!tab->buffer->IsDirty()) {
        tab->buffer->LoadFromFile(tab->filepath);
        if (tab->highlighter) {
            tab->highlighter->InvalidateLines(0, tab->buffer->GetLineCount());
        }
        return true;
    }
    return false;
}

void TabBar::ReloadAllFromDisk() {
    for (int i = static_cast<int>(tabs_.size()) - 1; i >= 0; --i) {
        auto& tab = tabs_[i];
        if (tab->filepath.empty() || tab->is_image) continue;

        if (!fs::exists(tab->filepath)) {
            if (!tab->buffer->IsDirty()) {
                CloseTab(i);
            }
        } else if (!tab->buffer->IsDirty()) {
            tab->buffer->LoadFromFile(tab->filepath);
            if (tab->highlighter) {
                tab->highlighter->InvalidateLines(0, tab->buffer->GetLineCount());
            }
        }
    }
}

void TabBar::ToggleActiveMarkdownPreview() {
    if (auto* tab = ActiveTab()) {
        std::string ext = platform::GetExtension(tab->filepath);
        if (ext == ".md" || ext == ".markdown" || tab->filepath.empty()) {
            tab->show_markdown_preview = !tab->show_markdown_preview;
        }
    }
}

/// Render the tab bar and the content of the active tab.
void TabBar::Render(const Theme* theme, ImFont* editor_font, ImFont* bold_font, ImFont* italic_font,
                    ImFont* h1_font, ImFont* h2_font) {
    if (tabs_.empty()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 50.0f && avail.y > 50.0f) {
            ImGui::BeginChild("##empty_editor_drop_area", avail, false);
            ImVec2 center = ImGui::GetCursorScreenPos();
            center.x += avail.x * 0.5f;
            center.y += avail.y * 0.5f;

            std::string empty_msg = "No files open";
            std::string hint_msg = "Drag & drop files here from Explorer or press Ctrl+O";
            ImVec2 sz1 = ImGui::CalcTextSize(empty_msg.c_str());
            ImVec2 sz2 = ImGui::CalcTextSize(hint_msg.c_str());

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(ImVec2(center.x - sz1.x * 0.5f, center.y - 20.0f), IM_COL32(160, 160, 160, 255), empty_msg.c_str());
            dl->AddText(ImVec2(center.x - sz2.x * 0.5f, center.y + 6.0f), IM_COL32(100, 100, 100, 255), hint_msg.c_str());

            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            if (payload && payload->IsDataType("LUCE_FILE")) {
                ImVec2 p_min = ImGui::GetWindowPos();
                ImVec2 p_max = ImVec2(p_min.x + avail.x, p_min.y + avail.y);
                if (ImGui::BeginDragDropTargetCustom(ImRect(p_min, p_max), ImGui::GetID("##empty_drop_target"))) {
                    RenderDropOverlay(p_min, p_max, "Open File", "Drop to open in editor", true);
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_FILE")) {
                        std::string fpath(static_cast<const char*>(p->Data));
                        OpenFile(fpath, theme);
                    }
                    ImGui::EndDragDropTarget();
                } else {
                    RenderDropOverlay(p_min, p_max, "Open File", nullptr, false);
                }
            }
            ImGui::EndChild();
        }
        return;
    }

    // Tab bar using ImGui's tab system.
    ImGuiTabBarFlags flags = ImGuiTabBarFlags_AutoSelectNewTabs |
                             ImGuiTabBarFlags_FittingPolicyScroll;

    if (ImGui::BeginTabBar("##file_tabs", flags)) {
        int selected_this_frame = -1;
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
            auto& tab = tabs_[i];
            std::string label = tab->title;
            if (tab->buffer->IsDirty()) label = "\xE2\x80\xA2 " + label;  // • main.cpp (Zed / VS Code style)
            if (tab->show_markdown_preview) label += " (Preview)";
            label += "###tab_" + std::to_string(i);

            bool open = true;
            ImGuiTabItemFlags item_flags = (tab_to_select_ >= 0 && i == tab_to_select_) ? ImGuiTabItemFlags_SetSelected : 0;
            bool tab_selected = ImGui::BeginTabItem(label.c_str(), &open, item_flags);
            if (tab_selected) {
                selected_this_frame = i;
                ImGui::EndTabItem();
            }

            // Drag Source for Tab
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("LUCE_TAB", &i, sizeof(int));
                ImTextureID icon = IconManager::Instance().GetIconForFile(tab->filepath.empty() ? tab->title : tab->filepath);
                if (icon) {
                    ImGui::Image(icon, ImVec2(16, 16));
                    ImGui::SameLine();
                }
                ImGui::Text("%s%s", tab->buffer->IsDirty() ? "• " : "", tab->title.c_str());
                ImGui::EndDragDropSource();
            }

            // Drop Target for Tab (Reordering & File Drop)
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_TAB")) {
                    int src_idx = *static_cast<const int*>(p->Data);
                    MoveTab(src_idx, i);
                }
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_FILE")) {
                    std::string fpath(static_cast<const char*>(p->Data));
                    OpenFile(fpath, theme);
                    if (active_ != i && active_ >= 0) {
                        MoveTab(active_, i);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (!open) {
                CloseTab(i);
                --i;
            }
        }

        // Add Trailing '+' button as TabItemButton
        if (ImGui::TabItemButton("+##add_tab", ImGuiTabItemFlags_Trailing)) {
            NewFile(theme);
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_TAB")) {
                int src_idx = *static_cast<const int*>(p->Data);
                MoveTab(src_idx, static_cast<int>(tabs_.size()) - 1);
            }
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_FILE")) {
                std::string fpath(static_cast<const char*>(p->Data));
                OpenFile(fpath, theme);
            }
            ImGui::EndDragDropTarget();
        }

        if (tab_to_select_ >= 0) {
            active_ = tab_to_select_;
            tab_to_select_ = -1;
        } else if (selected_this_frame >= 0) {
            active_ = selected_this_frame;
        }

        ImGui::EndTabBar();
    }

    if (!split_view_) {
        // Standard single editor / markdown preview view
        ImVec2 canvas_min = ImGui::GetCursorScreenPos();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 canvas_max = ImVec2(canvas_min.x + avail.x, canvas_min.y + avail.y);

        if (auto* tab = ActiveTab()) {
            std::string editor_id = "editor_" + std::to_string(active_);

            if (tab->is_image) {
                ImGui::BeginChild(editor_id.c_str(), ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                if (tab->image_texture) {
                    ImVec2 avail_img = ImGui::GetContentRegionAvail();
                    float scale = (std::min)(avail_img.x / tab->image_width, avail_img.y / tab->image_height);
                    if (scale > 1.0f) scale = 1.0f; // Don't scale up small images
                    
                    ImVec2 img_size(tab->image_width * scale, tab->image_height * scale);
                    // Center the image
                    ImVec2 cursor_pos = ImGui::GetCursorPos();
                    cursor_pos.x += (std::max)(0.0f, (avail_img.x - img_size.x) * 0.5f);
                    cursor_pos.y += (std::max)(0.0f, (avail_img.y - img_size.y) * 0.5f);
                    ImGui::SetCursorPos(cursor_pos);
                    
                    ImGui::Image((ImTextureID)(intptr_t)tab->image_texture, img_size);
                } else {
                    ImGui::Text("Failed to load image");
                }
                ImGui::EndChild();
            } else {
                tab->editor.SetTheme(theme);
                if (tab->show_markdown_preview) {
                    // Side-by-side split: Editor on left (Lilex font), Preview on right (IBM Plex Sans)
                    float half_w = ImGui::GetContentRegionAvail().x * 0.5f;
                    ImGui::BeginChild("##md_split_editor", ImVec2(half_w, 0), false);
                    if (editor_font) ImGui::PushFont(editor_font);
                    tab->editor.Render(editor_id.c_str());
                    if (editor_font) ImGui::PopFont();
                    ImGui::EndChild();

                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();

                    ImGui::BeginChild("##md_split_preview", ImVec2(0, 0), false);
                    tab->markdown_preview.Render("##md_prev", tab->buffer.get(), *theme,
                                                 bold_font, italic_font, h1_font, h2_font);
                    ImGui::EndChild();
                } else {
                    if (editor_font) ImGui::PushFont(editor_font);
                    tab->editor.Render(editor_id.c_str());
                    if (editor_font) ImGui::PopFont();
                }
            }
        }

        // Overlay drop targets for single editor
        HandleEditorDropTargets(theme, canvas_min, canvas_max);
    } else {
        // Split View (Side-by-side editing)
        Tab* left_tab = (active_ >= 0 && active_ < static_cast<int>(tabs_.size())) ? tabs_[active_].get() : nullptr;

        if (!tabs_.empty()) {
            if (split_active_ < 0 || split_active_ >= static_cast<int>(tabs_.size())) {
                split_active_ = active_;
            }
        } else {
            split_active_ = -1;
        }

        Tab* right_tab = (split_active_ >= 0 && split_active_ < static_cast<int>(tabs_.size())) ? tabs_[split_active_].get() : nullptr;

        float avail_w = ImGui::GetContentRegionAvail().x;
        float half_w = (avail_w - 6.0f) * 0.5f;
        float header_h = 24.0f;

        // ── LEFT PANE ──────────────────────────────────────────
        ImVec2 left_pane_min = ImGui::GetCursorScreenPos();
        ImGui::BeginChild("##split_pane_left", ImVec2(half_w, 0), false);
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            focused_pane_ = 0;
        }

        // Left pane header
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(theme->gutter_bg.x * 0.9f, theme->gutter_bg.y * 0.9f, theme->gutter_bg.z * 0.9f, 0.4f));
        ImGui::BeginChild("##left_pane_bar", ImVec2(0, header_h), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::AlignTextToFramePadding();
        ImVec4 active_indicator = (focused_pane_ == 0) ? ImVec4(0.2f, 0.7f, 1.0f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImGui::TextColored(active_indicator, " [Left Pane] ");
        ImGui::SameLine();
        if (left_tab) {
            ImGui::Text("%s%s", left_tab->buffer->IsDirty() ? "\xE2\x80\xA2 " : "", left_tab->title.c_str());
        } else {
            ImGui::TextDisabled("Empty");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Left pane editor
        if (left_tab) {
            std::string id = "editor_left_" + std::to_string(active_);
            left_tab->editor.SetTheme(theme);
            if (editor_font) ImGui::PushFont(editor_font);
            left_tab->editor.Render(id.c_str());
            if (editor_font) ImGui::PopFont();
        }
        ImGui::EndChild();
        ImVec2 left_pane_max = ImGui::GetItemRectMax();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // ── RIGHT PANE ─────────────────────────────────────────
        ImVec2 right_pane_min = ImGui::GetCursorScreenPos();
        ImGui::BeginChild("##split_pane_right", ImVec2(0, 0), false);
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            focused_pane_ = 1;
        }

        // Right pane header
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(theme->gutter_bg.x * 0.9f, theme->gutter_bg.y * 0.9f, theme->gutter_bg.z * 0.9f, 0.4f));
        ImGui::BeginChild("##right_pane_bar", ImVec2(0, header_h), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::AlignTextToFramePadding();
        ImVec4 right_indicator = (focused_pane_ == 1) ? ImVec4(0.2f, 0.7f, 1.0f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImGui::TextColored(right_indicator, " [Right Pane] ");
        ImGui::SameLine();

        float combo_w = (std::min)(220.0f, ImGui::GetContentRegionAvail().x - 40.0f);
        if (combo_w > 60.0f && !tabs_.empty()) {
            ImGui::SetNextItemWidth(combo_w);
            std::string current_title = right_tab ? (right_tab->title + (right_tab->buffer->IsDirty() ? " *" : "")) : "Select File...";
            if (ImGui::BeginCombo("##right_file_combo", current_title.c_str())) {
                for (size_t i = 0; i < tabs_.size(); ++i) {
                    bool sel = (static_cast<int>(i) == split_active_);
                    std::string label = tabs_[i]->title + (tabs_[i]->buffer->IsDirty() ? " *" : "") + "###rt_" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), sel)) {
                        split_active_ = static_cast<int>(i);
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 26.0f);
        if (ImGui::SmallButton("X##close_split")) {
            ToggleSplitView();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close Split View (Ctrl+\\)");

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Right pane editor / preview
        if (right_tab) {
            if (right_tab->show_markdown_preview) {
                right_tab->split_markdown_preview.Render("##md_prev_split", right_tab->buffer.get(), *theme,
                                                          bold_font, italic_font, h1_font, h2_font);
            } else {
                std::string id = "editor_right_" + std::to_string(split_active_);
                right_tab->split_editor.SetTheme(theme);
                if (editor_font) ImGui::PushFont(editor_font);
                right_tab->split_editor.Render(id.c_str());
                if (editor_font) ImGui::PopFont();
            }
        } else {
            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
            ImGui::TextDisabled("No second file open.");
            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
            if (ImGui::Button("New File##split_new")) {
                NewFile(theme);
                split_active_ = static_cast<int>(tabs_.size()) - 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Open File...##split_open")) {
                std::string p = platform::OpenFileDialog();
                if (!p.empty()) {
                    OpenFile(p, theme);
                }
            }
        }
        ImGui::EndChild();
        ImVec2 right_pane_max = ImGui::GetItemRectMax();

        // Handle Split View Drop Targets
        HandleSplitPaneDropTargets(theme, left_pane_min, left_pane_max, right_pane_min, right_pane_max);
    }
}

Tab* TabBar::ActiveTab() {
    if (split_view_ && focused_pane_ == 1 && split_active_ >= 0 && split_active_ < static_cast<int>(tabs_.size())) {
        return tabs_[split_active_].get();
    }
    if (active_ >= 0 && active_ < static_cast<int>(tabs_.size()))
        return tabs_[active_].get();
    return nullptr;
}

EditorView* TabBar::ActiveEditor() {
    auto* tab = ActiveTab();
    if (!tab) return nullptr;
    if (split_view_ && focused_pane_ == 1) {
        return &tab->split_editor;
    }
    return &tab->editor;
}

void TabBar::ToggleSplitView() {
    split_view_ = !split_view_;
    if (split_view_) {
        if (!tabs_.empty()) {
            if (split_active_ < 0 || split_active_ >= static_cast<int>(tabs_.size())) {
                split_active_ = active_;
            }
        } else {
            split_active_ = -1;
        }
    } else {
        focused_pane_ = 0;
    }
}

void TabBar::SetSplitView(bool enabled) {
    if (split_view_ != enabled) {
        ToggleSplitView();
    }
}

void TabBar::SetSplitActiveIndex(int idx) {
    if (idx >= 0 && idx < static_cast<int>(tabs_.size())) {
        split_active_ = idx;
    }
}

bool TabBar::HasUnsaved() const {
    for (auto& t : tabs_) {
        if (t->buffer->IsDirty()) return true;
    }
    return false;
}

void TabBar::NextTab() {
    if (!tabs_.empty()) {
        active_ = (active_ + 1) % static_cast<int>(tabs_.size());
        tab_to_select_ = active_;
    }
}

void TabBar::PrevTab() {
    if (!tabs_.empty()) {
        active_ = (active_ - 1 + static_cast<int>(tabs_.size())) % static_cast<int>(tabs_.size());
        tab_to_select_ = active_;
    }
}

void TabBar::SetMinimapEnabled(bool enabled) {
    show_minimap_ = enabled;
    for (auto& tab : tabs_) {
        tab->editor.show_minimap = enabled;
        tab->split_editor.show_minimap = enabled;
    }
}

void TabBar::SetSymbolIndex(SymbolIndex* index) {
    symbol_index_ = index;
    for (auto& tab : tabs_) {
        tab->editor.SetSymbolIndex(index);
        tab->split_editor.SetSymbolIndex(index);
    }
}

void TabBar::SetOnGoToDefinition(std::function<void(const std::string&, int)> cb) {
    on_goto_definition_ = std::move(cb);
    for (auto& tab : tabs_) {
        tab->editor.SetOnGoToDefinition(on_goto_definition_);
        tab->split_editor.SetOnGoToDefinition(on_goto_definition_);
    }
}

void TabBar::MoveTab(int from_idx, int to_idx) {
    if (from_idx < 0 || from_idx >= static_cast<int>(tabs_.size())) return;
    if (to_idx < 0 || to_idx >= static_cast<int>(tabs_.size())) return;
    if (from_idx == to_idx) return;

    auto moved_tab = std::move(tabs_[from_idx]);
    tabs_.erase(tabs_.begin() + from_idx);
    tabs_.insert(tabs_.begin() + to_idx, std::move(moved_tab));

    if (active_ == from_idx) {
        active_ = to_idx;
    } else if (from_idx < active_ && to_idx >= active_) {
        active_--;
    } else if (from_idx > active_ && to_idx <= active_) {
        active_++;
    }

    if (split_active_ == from_idx) {
        split_active_ = to_idx;
    } else if (from_idx < split_active_ && to_idx >= split_active_) {
        split_active_--;
    } else if (from_idx > split_active_ && to_idx <= split_active_) {
        split_active_++;
    }

    tab_to_select_ = active_;
}

void TabBar::SplitTabToSide(int tab_idx, const Theme* theme) {
    if (tab_idx < 0 || tab_idx >= static_cast<int>(tabs_.size())) return;

    split_view_ = true;
    split_active_ = tab_idx;
    focused_pane_ = 1;
}

void TabBar::OpenFileToSide(const std::string& path, const Theme* theme) {
    split_view_ = true;
    focused_pane_ = 1;
    OpenFile(path, theme);
}

void TabBar::MoveTabToPane(int tab_idx, int pane_idx) {
    if (tab_idx < 0 || tab_idx >= static_cast<int>(tabs_.size())) return;
    if (pane_idx == 0) {
        active_ = tab_idx;
        tab_to_select_ = tab_idx;
        focused_pane_ = 0;
    } else {
        split_active_ = tab_idx;
        focused_pane_ = 1;
    }
}

void TabBar::RenderDropOverlay(const ImVec2& min_pos, const ImVec2& max_pos, const char* title, const char* subtitle, bool hovered) {
    ImDrawList* fg = ImGui::GetForegroundDrawList();

    // Background tint
    ImU32 fill_col = hovered ? IM_COL32(0, 122, 204, 65) : IM_COL32(0, 122, 204, 20);
    fg->AddRectFilled(min_pos, max_pos, fill_col, 4.0f);

    // Outline border
    ImU32 border_col = hovered ? IM_COL32(0, 150, 255, 230) : IM_COL32(0, 122, 204, 110);
    fg->AddRect(min_pos, max_pos, border_col, 4.0f, 0, hovered ? 2.5f : 1.5f);

    // Centered Badge
    ImVec2 center((min_pos.x + max_pos.x) * 0.5f, (min_pos.y + max_pos.y) * 0.5f);
    ImVec2 title_sz = ImGui::CalcTextSize(title);
    ImVec2 sub_sz = subtitle ? ImGui::CalcTextSize(subtitle) : ImVec2(0, 0);

    float badge_w = (std::max)(title_sz.x, sub_sz.x) + 36.0f;
    float badge_h = title_sz.y + (subtitle ? sub_sz.y + 6.0f : 0.0f) + 20.0f;

    ImVec2 badge_min(center.x - badge_w * 0.5f, center.y - badge_h * 0.5f);
    ImVec2 badge_max(center.x + badge_w * 0.5f, center.y + badge_h * 0.5f);

    ImU32 badge_bg = hovered ? IM_COL32(20, 26, 38, 235) : IM_COL32(25, 30, 42, 200);
    ImU32 badge_border = hovered ? IM_COL32(0, 150, 255, 255) : IM_COL32(0, 122, 204, 150);
    fg->AddRectFilled(badge_min, badge_max, badge_bg, 6.0f);
    fg->AddRect(badge_min, badge_max, badge_border, 6.0f, 0, 1.5f);

    ImVec2 text_pos(center.x - title_sz.x * 0.5f, badge_min.y + 10.0f);
    fg->AddText(text_pos, IM_COL32(255, 255, 255, 255), title);
    if (subtitle) {
        ImVec2 sub_pos(center.x - sub_sz.x * 0.5f, text_pos.y + title_sz.y + 4.0f);
        fg->AddText(sub_pos, IM_COL32(180, 205, 230, 220), subtitle);
    }
}

void TabBar::HandleEditorDropTargets(const Theme* theme, const ImVec2& canvas_min, const ImVec2& canvas_max) {
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (!payload) return;
    if (!payload->IsDataType("LUCE_TAB") && !payload->IsDataType("LUCE_FILE")) return;

    float total_w = canvas_max.x - canvas_min.x;
    float total_h = canvas_max.y - canvas_min.y;
    if (total_w < 100.0f || total_h < 50.0f) return;

    float split_zone_w = (std::min)(total_w * 0.35f, 320.0f);
    if (split_zone_w < 80.0f) split_zone_w = total_w * 0.5f;

    ImVec2 main_min = canvas_min;
    ImVec2 main_max = ImVec2(canvas_max.x - split_zone_w, canvas_max.y);
    ImVec2 split_min = ImVec2(canvas_max.x - split_zone_w, canvas_min.y);
    ImVec2 split_max = canvas_max;

    // 1. Left/Main Drop Zone (for opening file in the current editor)
    if (payload->IsDataType("LUCE_FILE")) {
        if (ImGui::BeginDragDropTargetCustom(ImRect(main_min, main_max), ImGui::GetID("##editor_main_drop_zone"))) {
            RenderDropOverlay(main_min, main_max, "Open in Active Editor", "Drop to open file", true);
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_FILE")) {
                std::string path(static_cast<const char*>(p->Data));
                OpenFile(path, theme);
            }
            ImGui::EndDragDropTarget();
        }
    }

    // 2. Right Split Drop Zone (drag tab or file to open side-by-side)
    if (ImGui::BeginDragDropTargetCustom(ImRect(split_min, split_max), ImGui::GetID("##editor_split_drop_zone"))) {
        RenderDropOverlay(split_min, split_max, "Split Right", "Drop to open to the side", true);
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_TAB")) {
            int src_idx = *static_cast<const int*>(p->Data);
            SplitTabToSide(src_idx, theme);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_FILE")) {
            std::string path(static_cast<const char*>(p->Data));
            OpenFileToSide(path, theme);
        }
        ImGui::EndDragDropTarget();
    } else {
        RenderDropOverlay(split_min, split_max, "Split Right", nullptr, false);
    }
}

void TabBar::HandleSplitPaneDropTargets(const Theme* theme, const ImVec2& left_min, const ImVec2& left_max,
                                       const ImVec2& right_min, const ImVec2& right_max) {
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (!payload) return;
    if (!payload->IsDataType("LUCE_TAB") && !payload->IsDataType("LUCE_FILE")) return;

    // 1. Left Pane Drop Target
    if (ImGui::BeginDragDropTargetCustom(ImRect(left_min, left_max), ImGui::GetID("##split_left_drop_zone"))) {
        RenderDropOverlay(left_min, left_max, "Left Pane", "Drop to open in left view", true);
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_TAB")) {
            int src_idx = *static_cast<const int*>(p->Data);
            MoveTabToPane(src_idx, 0);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_FILE")) {
            std::string path(static_cast<const char*>(p->Data));
            focused_pane_ = 0;
            OpenFile(path, theme);
        }
        ImGui::EndDragDropTarget();
    } else {
        RenderDropOverlay(left_min, left_max, "Left Pane", nullptr, false);
    }

    // 2. Right Pane Drop Target
    if (ImGui::BeginDragDropTargetCustom(ImRect(right_min, right_max), ImGui::GetID("##split_right_drop_zone"))) {
        RenderDropOverlay(right_min, right_max, "Right Pane", "Drop to open in right view", true);
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_TAB")) {
            int src_idx = *static_cast<const int*>(p->Data);
            MoveTabToPane(src_idx, 1);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("LUCE_FILE")) {
            std::string path(static_cast<const char*>(p->Data));
            OpenFileToSide(path, theme);
        }
        ImGui::EndDragDropTarget();
    } else {
        RenderDropOverlay(right_min, right_max, "Right Pane", nullptr, false);
    }
}

}  // namespace luce
