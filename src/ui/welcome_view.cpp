// ============================================================================
// WelcomeView — Implementation.
// ============================================================================

#include "welcome_view.h"
#include "app.h"
#include "platform.h"
#include "icon_manager.h"
#include "settings_manager.h"
#include "settings_view.h"
#include "command_palette.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace luce {

WelcomeView::WelcomeView() = default;

void WelcomeView::Render(App* app, const Theme* theme, ImFont* bold_font,
                         ImFont* italic_font, ImFont* h1_font, ImFont* h2_font) {
    if (!app || !theme) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme->background);
    ImGui::BeginChild("##welcome_main_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_None);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float max_w = 760.0f;
    float content_w = (avail_w - 48.0f > max_w) ? max_w : (avail_w - 48.0f);
    if (content_w < 320.0f) content_w = (std::max)(avail_w - 16.0f, 260.0f);

    float pad_x = (std::max)(16.0f, (avail_w - content_w) * 0.5f);

    ImGui::SetCursorPosX(pad_x);
    ImGui::Dummy(ImVec2(content_w, 0.0f));
    ImGui::SetCursorPosX(pad_x);
    ImGui::BeginGroup();

    // ── 1. Header ─────────────────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, 24.0f));

    if (h1_font) ImGui::PushFont(h1_font);
    else if (bold_font) ImGui::PushFont(bold_font);
    ImGui::TextColored(theme->foreground, "Welcome to Luce");
    if (h1_font || bold_font) ImGui::PopFont();

    ImGui::Spacing();
    ImGui::TextColored(theme->gutter_fg, "Fast, lightweight C++23 code editor with native UI");
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── 2. Content Grid (Two Columns if wide enough, otherwise stacked) ───────
    bool two_columns = (content_w >= 620.0f);
    float col_gap = 28.0f;
    float col_w = two_columns ? ((content_w - col_gap) * 0.5f) : content_w;

    auto& icons = IconManager::Instance();
    ImTextureID icon_file     = icons.GetIconByName("default_file");
    ImTextureID icon_folder   = icons.GetFolderIcon(false);
    ImTextureID icon_git      = icons.GetIconByName("file_type_git");
    ImTextureID icon_cmd      = icons.GetIconByName("file_type_config");
    ImTextureID icon_keys     = icons.GetIconByName("file_type_key");
    ImTextureID icon_plugins  = icons.GetIconByName("folder_type_plugin");
    ImTextureID icon_theme    = icons.GetIconByName("file_type_css");
    ImTextureID icon_docs     = icons.GetIconByName("file_type_docusaurus");
    if (!icon_docs) icon_docs = icons.GetIconByName("file_type_markdown");
    ImTextureID icon_issues   = icons.GetIconByName("file_type_log");

    // ── LEFT COLUMN ───────────────────────────────────────────────────────────
    ImGui::BeginGroup();
    {
        // GET STARTED
        RenderSectionHeader("GET STARTED", theme, bold_font);

        if (RenderActionRow("##act_new_file", icon_file, "New File", "Ctrl+N", theme, col_w, bold_font)) {
            app->GetTabBar().NewFile(&app->GetThemeManager().Active());
            app->SaveSession();
        }

        if (RenderActionRow("##act_open_folder", icon_folder, "Open Folder...", "Ctrl+O", theme, col_w, bold_font)) {
            std::string folder = platform::OpenFolderDialog();
            if (!folder.empty()) {
                app->OpenWorkspaceFolder(folder);
            }
        }

        if (RenderActionRow("##act_clone_repo", icon_git, "Clone Repository...", "Git", theme, col_w, bold_font)) {
            app->ShowGitCloneModal();
        }

        if (RenderActionRow("##act_cmd_palette", icon_cmd, "Open Command Palette", "Ctrl+Shift+P", theme, col_w, bold_font)) {
            app->GetCommandPalette().Open(PaletteMode::Commands);
        }

        ImGui::Dummy(ImVec2(0, 16.0f));

        // RECENT WORKSPACES
        RenderSectionHeader("RECENT WORKSPACES", theme, bold_font);

        const auto& recent = app->GetSettingsManager().Get().recent_projects;
        if (recent.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
            ImGui::TextUnformatted("No recent folders opened yet.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        } else {
            size_t count = (std::min)(recent.size(), size_t(5));
            for (size_t i = 0; i < count; ++i) {
                std::string row_id = "##recent_" + std::to_string(i);
                if (RenderRecentRow(row_id.c_str(), recent[i], theme, col_w, bold_font)) {
                    app->OpenWorkspaceFolder(recent[i]);
                }
            }
        }
    }
    ImGui::EndGroup();

    // ── RIGHT COLUMN ──────────────────────────────────────────────────────────
    if (two_columns) {
        ImGui::SameLine(0.0f, col_gap);
    } else {
        ImGui::Dummy(ImVec2(0, 16.0f));
    }

    ImGui::BeginGroup();
    {
        // CONFIGURATION
        RenderSectionHeader("CONFIGURATION", theme, bold_font);

        if (RenderActionRow("##act_settings", icon_cmd, "Open Settings", "Ctrl+,", theme, col_w, bold_font)) {
            app->OpenSettingsWindow();
        }

        if (RenderActionRow("##act_keybindings", icon_keys, "Keyboard Shortcuts", "Keys", theme, col_w, bold_font)) {
            app->OpenSettingsWindow();
            app->GetSettingsView().SetCategory(SettingsCategory::Keybindings);
        }

        if (RenderActionRow("##act_plugins", icon_plugins, "Explore Extensions", "Plugins", theme, col_w, bold_font)) {
            app->ShowPluginsPanel();
        }

        if (RenderActionRow("##act_themes", icon_theme, "Color Themes", "Theme", theme, col_w, bold_font)) {
            app->GetCommandPalette().OpenWithText("Theme: ");
        }

        ImGui::Dummy(ImVec2(0, 16.0f));

        // HELP & RESOURCES
        RenderSectionHeader("HELP & RESOURCES", theme, bold_font);

        if (RenderActionRow("##act_docs", icon_docs, "Documentation", "Docs", theme, col_w, bold_font)) {
            platform::OpenURL("https://luce-editor.github.io");
        }

        if (RenderActionRow("##act_repo", icon_git, "GitHub Repository", "GitHub", theme, col_w, bold_font)) {
            platform::OpenURL("https://github.com/luce-editor/luce");
        }

        if (RenderActionRow("##act_issues", icon_issues, "Report an Issue", "Issues", theme, col_w, bold_font)) {
            platform::OpenURL("https://github.com/luce-editor/luce/issues");
        }
    }
    ImGui::EndGroup();

    // ── 3. Footer ─────────────────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, 24.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10.0f));

    if (ImGui::BeginTable("##welcome_footer_tbl", 2, ImGuiTableFlags_None, ImVec2(content_w, 0.0f))) {
        ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 48.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (bold_font) ImGui::PushFont(bold_font);
        ImGui::TextColored(theme->foreground, "Show Welcome page on startup");
        if (bold_font) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(theme->gutter_fg.x, theme->gutter_fg.y, theme->gutter_fg.z, 0.85f));
        ImGui::TextWrapped("When enabled, the Welcome screen will open whenever no project files are open.");
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.0f);
        auto& settings = app->GetSettingsManager().GetMutable();
        if (ToggleSwitch("##show_welcome_toggle", &settings.show_welcome_on_startup, theme, 22.0f)) {
            app->GetTabBar().SetShowWelcomeOnStartup(settings.show_welcome_on_startup);
            app->GetSettingsManager().SaveToFile();
        }

        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0, 30.0f));

    ImGui::EndGroup(); // End center content group

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
}

void WelcomeView::RenderSectionHeader(const char* title, const Theme* theme, ImFont* bold_font) {
    ImGui::Spacing();
    // Styling matching NO FOLDER OPENED in FileExplorer and Git panels
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.60f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

bool WelcomeView::RenderActionRow(const char* id, ImTextureID icon, const char* title,
                                  const char* shortcut, const Theme* theme, float width,
                                  ImFont* bold_font) {
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float height = 34.0f;
    ImVec2 size(width, height);

    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background on hover or active (matching Explorer list item hover)
    if (active) {
        dl->AddRectFilled(p0, ImVec2(p0.x + width, p0.y + height),
                          ImGui::ColorConvertFloat4ToU32(theme->active_line), 4.0f);
    } else if (hovered) {
        ImVec4 hov_col = ImVec4(theme->active_line.x, theme->active_line.y, theme->active_line.z, 0.45f);
        dl->AddRectFilled(p0, ImVec2(p0.x + width, p0.y + height),
                          ImGui::ColorConvertFloat4ToU32(hov_col), 4.0f);
    }

    // Left Icon (crisp SVG texture)
    float text_x = p0.x + 10.0f;
    if (icon) {
        float icon_sz = 16.0f;
        float icon_y = p0.y + (height - icon_sz) * 0.5f;
        dl->AddImage(icon, ImVec2(text_x, icon_y), ImVec2(text_x + icon_sz, icon_y + icon_sz));
        text_x += icon_sz + 10.0f;
    }

    // Title label
    float text_y = p0.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    ImU32 text_col = ImGui::ColorConvertFloat4ToU32(hovered ? theme->foreground : ImVec4(theme->foreground.x, theme->foreground.y, theme->foreground.z, 0.92f));
    dl->AddText(ImVec2(text_x, text_y), text_col, title);

    // Shortcut badge / hint aligned on the right
    if (shortcut && shortcut[0] != '\0') {
        ImVec2 sc_sz = ImGui::CalcTextSize(shortcut);
        float sc_x = p0.x + width - sc_sz.x - 12.0f;
        ImU32 sc_col = ImGui::ColorConvertFloat4ToU32(theme->gutter_fg);
        dl->AddText(ImVec2(sc_x, text_y), sc_col, shortcut);
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    return clicked;
}

bool WelcomeView::RenderRecentRow(const char* id, const std::string& folder_path,
                                  const Theme* theme, float width, ImFont* bold_font) {
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float height = 38.0f;
    ImVec2 size(width, height);

    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("%s", folder_path.c_str());
    }
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (active) {
        dl->AddRectFilled(p0, ImVec2(p0.x + width, p0.y + height),
                          ImGui::ColorConvertFloat4ToU32(theme->active_line), 4.0f);
    } else if (hovered) {
        ImVec4 hov_col = ImVec4(theme->active_line.x, theme->active_line.y, theme->active_line.z, 0.45f);
        dl->AddRectFilled(p0, ImVec2(p0.x + width, p0.y + height),
                          ImGui::ColorConvertFloat4ToU32(hov_col), 4.0f);
    }

    ImTextureID folder_icon = IconManager::Instance().GetFolderIcon(false);
    float text_x = p0.x + 10.0f;
    if (folder_icon) {
        float icon_sz = 18.0f;
        float icon_y = p0.y + (height - icon_sz) * 0.5f;
        dl->AddImage(folder_icon, ImVec2(text_x, icon_y), ImVec2(text_x + icon_sz, icon_y + icon_sz));
        text_x += icon_sz + 10.0f;
    }

    // Basename and parent directory
    std::string folder_name = platform::GetFilename(folder_path);
    if (folder_name.empty()) folder_name = folder_path;
    std::string parent_dir = platform::GetDirectory(folder_path);

    float name_y = p0.y + 4.0f;
    ImU32 text_col = ImGui::ColorConvertFloat4ToU32(theme->foreground);
    dl->AddText(ImVec2(text_x, name_y), text_col, folder_name.c_str());

    float path_y = name_y + ImGui::GetTextLineHeight() - 1.0f;
    ImU32 path_col = ImGui::ColorConvertFloat4ToU32(theme->gutter_fg);

    // Truncate path if too long
    float max_path_w = width - (text_x - p0.x) - 12.0f;
    std::string disp_path = parent_dir;
    if (ImGui::CalcTextSize(disp_path.c_str()).x > max_path_w && disp_path.size() > 20) {
        while (disp_path.size() > 10 && ImGui::CalcTextSize((disp_path + "...").c_str()).x > max_path_w) {
            disp_path.pop_back();
        }
        disp_path += "...";
    }
    dl->AddText(ImVec2(text_x, path_y), path_col, disp_path.c_str());

    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    return clicked;
}

} // namespace luce
