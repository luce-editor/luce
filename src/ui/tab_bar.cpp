// ============================================================================
// TabBar — Implementation.
// ============================================================================

#include "tab_bar.h"
#include "platform.h"
#include "../editor/git_manager.h"
#include "../editor/diagnostic_runner.h"

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
    // Check if already open.
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        if (tabs_[i]->filepath == path) {
            active_ = i;
            return;
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

        // Wire up the change callback so edits invalidate the syntax cache.
        SyntaxHighlighter* hl = tab->highlighter.get();
        tab->buffer->SetChangeCallback([hl](int line, int count) {
            hl->InvalidateLines(line, count);
        });

        // Auto-detect language from extension or full filename (e.g. CMakeLists.txt)
        if (!tab->highlighter->SetLanguageByExtension(ext)) {
            tab->highlighter->SetLanguageByExtension(tab->title);
        }

        tab->editor.SetBuffer(tab->buffer.get());
        tab->editor.SetHighlighter(tab->highlighter.get());
        tab->editor.SetTheme(theme);
        tab->editor.SetFilePath(path);
    }

    tabs_.push_back(std::move(tab));
    active_ = static_cast<int>(tabs_.size()) - 1;
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

    tabs_.push_back(std::move(tab));
    active_ = static_cast<int>(tabs_.size()) - 1;
}

bool TabBar::SaveActive() {
    auto* tab = ActiveTab();
    if (!tab) return false;

    if (tab->filepath.empty()) {
        std::string path = platform::SaveFileDialog();
        if (path.empty()) return false;
        return SaveActiveAs(path);
    }

    bool ok = tab->buffer->SaveToFile(tab->filepath);
    if (ok) {
        tab->buffer->ClearDirty();
        GitManager::Instance().Refresh();
        DiagnosticRunner::Instance().CheckFile(tab->filepath);
    }
    return ok;
}

bool TabBar::SaveActiveAs(const std::string& path) {
    auto* tab = ActiveTab();
    if (!tab) return false;

    bool ok = tab->buffer->SaveToFile(path);
    if (ok) {
        tab->filepath = path;
        tab->title    = platform::GetFilename(path);
        tab->editor.SetFilePath(path);
        tab->buffer->ClearDirty();
        GitManager::Instance().Refresh();
        DiagnosticRunner::Instance().CheckFile(tab->filepath);

        // Re-detect language.
        std::string ext = platform::GetExtension(path);
        tab->highlighter->SetLanguageByExtension(ext);
    }
    return ok;
}

bool TabBar::CloseTab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return true;
    
    if (tabs_[index]->is_image && tabs_[index]->image_texture) {
        GLuint tex = tabs_[index]->image_texture;
        glDeleteTextures(1, &tex);
    }
    
    // TODO: prompt to save if dirty.
    tabs_.erase(tabs_.begin() + index);
    if (active_ >= static_cast<int>(tabs_.size())) {
        active_ = static_cast<int>(tabs_.size()) - 1;
    }
    return true;
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
    if (tabs_.empty()) return;

    // Tab bar using ImGui's tab system.
    ImGuiTabBarFlags flags = ImGuiTabBarFlags_Reorderable |
                             ImGuiTabBarFlags_AutoSelectNewTabs |
                             ImGuiTabBarFlags_FittingPolicyScroll;

    if (ImGui::BeginTabBar("##file_tabs", flags)) {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
            auto& tab = tabs_[i];
            std::string label = tab->title;
            if (tab->buffer->IsDirty()) label = "\xE2\x80\xA2 " + label;  // • main.cpp (Zed / VS Code style)
            if (tab->show_markdown_preview) label += " (Preview)";
            label += "###tab_" + std::to_string(i);

            bool open = true;
            ImGuiTabItemFlags item_flags = 0;
            if (ImGui::BeginTabItem(label.c_str(), &open, item_flags)) {
                active_ = i;
                ImGui::EndTabItem();
            }
            if (!open) {
                CloseTab(i);
                --i;
            }
        }
        ImGui::EndTabBar();
    }

    // Render the active editor and/or markdown preview
    if (auto* tab = ActiveTab()) {
        std::string editor_id = "editor_" + std::to_string(active_);

        if (tab->is_image) {
            ImGui::BeginChild(editor_id.c_str(), ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            if (tab->image_texture) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float scale = std::min(avail.x / tab->image_width, avail.y / tab->image_height);
                if (scale > 1.0f) scale = 1.0f; // Don't scale up small images
                
                ImVec2 img_size(tab->image_width * scale, tab->image_height * scale);
                // Center the image
                ImVec2 cursor_pos = ImGui::GetCursorPos();
                cursor_pos.x += std::max(0.0f, (avail.x - img_size.x) * 0.5f);
                cursor_pos.y += std::max(0.0f, (avail.y - img_size.y) * 0.5f);
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
}

Tab* TabBar::ActiveTab() {
    if (active_ >= 0 && active_ < static_cast<int>(tabs_.size()))
        return tabs_[active_].get();
    return nullptr;
}

EditorView* TabBar::ActiveEditor() {
    auto* tab = ActiveTab();
    return tab ? &tab->editor : nullptr;
}

bool TabBar::HasUnsaved() const {
    for (auto& t : tabs_) {
        if (t->buffer->IsDirty()) return true;
    }
    return false;
}

void TabBar::NextTab() {
    if (!tabs_.empty()) active_ = (active_ + 1) % static_cast<int>(tabs_.size());
}

void TabBar::PrevTab() {
    if (!tabs_.empty()) active_ = (active_ - 1 + static_cast<int>(tabs_.size())) % static_cast<int>(tabs_.size());
}

}  // namespace luce
