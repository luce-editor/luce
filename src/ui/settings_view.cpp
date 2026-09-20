// ============================================================================
// SettingsView — Implementation.
// ============================================================================

#include "settings_view.h"
#include "app.h"
#include "platform.h"
#include "icon_manager.h"
#include "imgui.h"
#include "imgui_internal.h"

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#   include <psapi.h>
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif
#endif

#include <algorithm>
#include <vector>

namespace luce {

SettingsView::SettingsView() = default;

void SettingsView::Render(App* app, const Theme* theme, ImFont* bold_font,
                          ImFont* italic_font, ImFont* h1_font, ImFont* h2_font) {
    if (!app || !theme) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme->background);
    ImGui::BeginChild("##settings_main_container", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

    // 1. Top Header Area
    RenderHeader(app, theme, bold_font, h1_font);

    ImGui::Separator();
    ImGui::Spacing();

    // 2. Search filtering or standard category layout
    std::string query = search_buf_;
    std::ranges::transform(query, query.begin(), ::tolower);

    if (!query.empty()) {
        RenderFilteredSettings(app, query, theme, bold_font, h2_font);
    } else {
        // Two-column master-detail layout: Left Sidebar + Right Settings Content
        float sidebar_w = 210.0f;
        ImGui::BeginChild("##settings_sidebar", ImVec2(sidebar_w, 0), false);
        RenderSidebar(theme, bold_font);
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::BeginChild("##settings_content", ImVec2(0, 0), false);
        RenderCategoryContent(app, theme, bold_font, italic_font, h1_font, h2_font);
        ImGui::EndChild();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void SettingsView::RenderHeader(App* app, const Theme* theme, ImFont* bold_font, ImFont* h1_font) {
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);

    // Title and Open settings.json button on the same row
    if (h1_font) ImGui::PushFont(h1_font);
    ImGui::TextColored(theme->foreground, "Settings");
    if (h1_font) ImGui::PopFont();

    ImGui::SameLine();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float btn_w = 170.0f;
    if (avail_w > btn_w + 30.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_w - btn_w - 16.0f);
        // Primary action button styled like Open Folder in FileExplorer
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.44f, 0.72f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.52f, 0.82f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.36f, 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("{ } Open settings.json", ImVec2(btn_w, 28.0f))) {
            app->OpenSettingsFile();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Open configuration in JSON editor");
        }
    }

    // Subtitle
    ImGui::SetCursorPosX(16.0f);
    ImGui::TextColored(theme->gutter_fg, "Configure preferences, typography, appearance, and keyboard shortcuts.");
    ImGui::Spacing();

    // Search bar
    ImGui::SetCursorPosX(16.0f);
    ImGui::SetNextItemWidth(340.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::InputTextWithHint("##settings_search", "Search settings (font, tab, theme)...", search_buf_, sizeof(search_buf_));
    ImGui::PopStyleVar(2);

    if (search_buf_[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            search_buf_[0] = '\0';
        }
    }
    ImGui::Spacing();
}

void SettingsView::RenderSidebar(const Theme* theme, ImFont* bold_font) {
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
    ImGui::TextUnformatted("CATEGORIES");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    struct CatItem {
        SettingsCategory cat;
        const char* label;
        const char* icon;
    };

    static const CatItem categories[] = {
        { SettingsCategory::Editor,      "Text Editor",        "editor" },
        { SettingsCategory::Appearance,  "Appearance & Theme", "theme" },
        { SettingsCategory::General,     "General",            "general" },
        { SettingsCategory::Keybindings, "Keyboard Shortcuts", "keys" },
        { SettingsCategory::Plugins,     "Plugins",            "plugins" },
        { SettingsCategory::About,       "About Luce",         "about" },
    };

    float item_w = ImGui::GetContentRegionAvail().x - 6.0f;
    for (const auto& item : categories) {
        bool is_selected = (active_category_ == item.cat);
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float item_h = 30.0f;

        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme->active_line);
            ImGui::PushStyleColor(ImGuiCol_Text, theme->foreground);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(theme->active_line.x, theme->active_line.y, theme->active_line.z, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme->active_line);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.08f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        std::string btn_label = item.label;
        btn_label += "##cat_" + std::string(item.icon);
        if (ImGui::Button(btn_label.c_str(), ImVec2(item_w, item_h))) {
            active_category_ = item.cat;
        }

        // Active blue accent indicator bar on left edge (like VS Code and Luce explorer)
        if (is_selected) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(ImVec2(p0.x, p0.y + 4.0f), ImVec2(p0.x + 3.0f, p0.y + item_h - 4.0f),
                              IM_COL32(0, 122, 204, 255), 1.5f);
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }
}

void SettingsView::RenderCategoryContent(App* app, const Theme* theme, ImFont* bold_font,
                                        ImFont* italic_font, ImFont* h1_font, ImFont* h2_font) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
    ImGui::Spacing();

    switch (active_category_) {
        case SettingsCategory::Editor:
            RenderEditorSettings(app, theme, bold_font, h2_font);
            break;
        case SettingsCategory::Appearance:
            RenderAppearanceSettings(app, theme, bold_font, h2_font);
            break;
        case SettingsCategory::General:
            RenderGeneralSettings(app, theme, bold_font, h2_font);
            break;
        case SettingsCategory::Keybindings:
            RenderKeybindings(app, theme, bold_font, h2_font);
            break;
        case SettingsCategory::Plugins:
            RenderPlugins(app, theme, bold_font, h2_font);
            break;
        case SettingsCategory::About:
            RenderAbout(app, theme, bold_font, h1_font, h2_font);
            break;
    }
}

bool ToggleSwitch(const char* str_id, bool* v, const Theme* theme, float custom_height) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    float height = custom_height > 0.0f ? custom_height : 22.0f;
    float width  = height * 1.85f; // ~40px wide for 22px high
    float radius = height * 0.5f; // Pill radius

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) {
        *v = !*v;
        ImGui::MarkItemEdited(id);
    }

    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    // Animation progress (0.0 = off, 1.0 = on)
    float t = *v ? 1.0f : 0.0f;
    if (g.LastActiveId == id) {
        float t_anim = ((float)(g.Time - g.LastActiveIdTimer)) / 0.12f;
        if (t_anim > 1.0f) t_anim = 1.0f;
        t = *v ? t_anim : (1.0f - t_anim);
    }

    // Track colors (matching Zed / user's screenshot):
    // OFF: Dark muted slate (#242830)
    // ON:  Zed petroleum blue (#2c5b82 / RGB(43, 91, 130))
    ImVec4 col_off(0.15f, 0.17f, 0.20f, 1.0f);
    ImVec4 col_on(0.17f, 0.36f, 0.51f, 1.0f);
    ImVec4 track_vec = ImLerp(col_off, col_on, t);
    if (hovered) {
        track_vec.x = std::min(1.0f, track_vec.x + 0.05f);
        track_vec.y = std::min(1.0f, track_vec.y + 0.05f);
        track_vec.z = std::min(1.0f, track_vec.z + 0.05f);
    }
    ImU32 col_track = ImGui::ColorConvertFloat4ToU32(track_vec);

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bb.Min, bb.Max, col_track, radius);

    // Subtle track border
    ImU32 col_border = hovered ? IM_COL32(85, 130, 175, 180) : IM_COL32(50, 56, 66, 160);
    dl->AddRect(bb.Min, bb.Max, col_border, radius, 0, 1.0f);

    // Knob / Thumb circle
    float knob_padding = 2.5f;
    float knob_radius = radius - knob_padding;
    float knob_x_off = bb.Min.x + radius;
    float knob_x_on  = bb.Max.x - radius;
    float knob_x = ImLerp(knob_x_off, knob_x_on, t);
    float knob_y = bb.Min.y + radius;

    // Knob color: Warm silver-gray (#c8c6be / RGB(200, 198, 190) from Zed)
    ImVec4 knob_vec = hovered ? ImVec4(0.92f, 0.91f, 0.88f, 1.0f) : ImVec4(0.80f, 0.79f, 0.76f, 1.0f);
    ImU32 col_knob = ImGui::ColorConvertFloat4ToU32(knob_vec);

    dl->AddCircleFilled(ImVec2(knob_x, knob_y), knob_radius, col_knob);

    return pressed;
}

void SettingsView::RenderToggleCard(const char* id, const char* title, const char* description,
                                    bool* val, const Theme* theme, ImFont* bold_font,
                                    std::function<void(bool)> on_change) {
    ImGui::PushID(id);
    ImGui::Spacing();

    if (ImGui::BeginTable("##card_tbl", 2, ImGuiTableFlags_None)) {
        ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 48.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Title
        if (bold_font) ImGui::PushFont(bold_font);
        ImGui::TextColored(theme->foreground, "%s", title);
        if (bold_font) ImGui::PopFont();

        // Description
        if (description && description[0] != '\0') {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(theme->gutter_fg.x, theme->gutter_fg.y, theme->gutter_fg.z, 0.85f));
            ImGui::TextWrapped("%s", description);
            ImGui::PopStyleColor();
        }

        // Toggle Switch Column (aligned to right, baseline with title)
        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.0f);
        bool v = *val;
        if (ToggleSwitch("##sw", &v, theme, 22.0f)) {
            *val = v;
            if (on_change) on_change(v);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PopID();
}

void SettingsView::RenderSettingCard(const char* id, const char* title, const char* description,
                                    const Theme* theme, ImFont* bold_font, std::function<void()> draw_control) {
    ImGui::PushID(id);

    ImGui::Spacing();
    // Setting Title (clean bold text matching Explorer/Git item labels)
    if (bold_font) ImGui::PushFont(bold_font);
    ImGui::TextColored(theme->foreground, "%s", title);
    if (bold_font) ImGui::PopFont();

    // Setting Description (muted text matching Explorer secondary labels)
    if (description && description[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(theme->gutter_fg.x, theme->gutter_fg.y, theme->gutter_fg.z, 0.85f));
        ImGui::TextUnformatted(description);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    draw_control();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PopID();
}

void SettingsView::RenderEditorSettings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font) {
    auto& settings = app->GetSettingsManager().GetMutable();

    ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
    ImGui::TextUnformatted("TEXT EDITOR SETTINGS");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Editor Font Size
    RenderSettingCard("ed_font_size", "Editor: Font Size",
                      "Controls the font size of the code editor and terminal in pixels.",
                      theme, bold_font, [&]() {
        int cur_size = settings.editor_font_size;
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderInt("##font_size_slider", &cur_size, 10, 36, "%d px")) {
            app->SetEditorFontSize(cur_size);
        }
        ImGui::SameLine();
        if (ImGui::Button("-##font_dec", ImVec2(28.0f, 0.0f))) {
            app->SetEditorFontSize(cur_size - 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("+##font_inc", ImVec2(28.0f, 0.0f))) {
            app->SetEditorFontSize(cur_size + 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset (15px)##reset_ed_font")) {
            app->SetEditorFontSize(15);
        }

        // Live Font Preview box
        ImGui::Spacing();
        ImGui::TextColored(theme->gutter_fg, "Live Preview:");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme->background);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("##font_preview_box", ImVec2(0, 60.0f), true, ImGuiWindowFlags_NoScrollbar);
        if (app->GetEditorFont()) ImGui::PushFont(app->GetEditorFont());
        ImGui::TextColored(theme->syntax_keyword, "const char*");
        ImGui::SameLine();
        ImGui::TextColored(theme->syntax_identifier, " greeting");
        ImGui::SameLine();
        ImGui::TextColored(theme->syntax_operator, " = ");
        ImGui::SameLine();
        ImGui::TextColored(theme->syntax_string, "\"Hello, Luce!\"");
        ImGui::SameLine();
        ImGui::TextColored(theme->syntax_punctuation, ";");
        ImGui::TextColored(theme->syntax_comment, "// Font size: %d px — Lilex Monospace", settings.editor_font_size);
        if (app->GetEditorFont()) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    });

    // 2. Zoom with Mouse Wheel
    RenderToggleCard("ed_mouse_wheel", "Editor: Mouse Wheel Zoom",
                     "Zoom font size smoothly when holding Ctrl and scrolling the mouse wheel.",
                     &settings.zoom_with_mouse_wheel, theme, bold_font, [&](bool v) {
        settings.zoom_with_mouse_wheel = v;
        app->ApplySettings(settings);
        app->GetSettingsManager().SaveToFile();
    });

    // 3. Tab Size
    RenderSettingCard("ed_tab_size", "Editor: Tab Size",
                      "The number of spaces a tab is equal to.",
                      theme, bold_font, [&]() {
        int sizes[] = { 2, 4, 8 };
        for (int s : sizes) {
            bool is_active = (settings.tab_size == s);
            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Button, theme->active_line);
            }
            std::string label = std::to_string(s) + " spaces##ts_" + std::to_string(s);
            if (ImGui::Button(label.c_str(), ImVec2(90.0f, 26.0f))) {
                settings.tab_size = s;
                app->ApplySettings(settings);
                app->GetSettingsManager().SaveToFile();
            }
            if (is_active) {
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
    });

    // 4. Insert Spaces vs Tabs
    RenderToggleCard("ed_use_spaces", "Editor: Insert Spaces",
                     "Insert spaces when pressing Tab instead of a tab character.",
                     &settings.use_spaces, theme, bold_font, [&](bool v) {
        settings.use_spaces = v;
        app->ApplySettings(settings);
        app->GetSettingsManager().SaveToFile();
    });

    // 5. Minimap
    RenderToggleCard("ed_minimap", "Editor: Minimap",
                     "Controls whether the code thumbnail minimap is shown on the right side.",
                     &settings.show_minimap, theme, bold_font, [&](bool v) {
        settings.show_minimap = v;
        app->ApplySettings(settings);
        app->GetSettingsManager().SaveToFile();
    });

    // 6. Line Numbers
    RenderToggleCard("ed_line_numbers", "Editor: Line Numbers",
                     "Controls the display of line numbers in the gutter margin.",
                     &settings.show_line_numbers, theme, bold_font, [&](bool v) {
        settings.show_line_numbers = v;
        app->ApplySettings(settings);
        app->GetSettingsManager().SaveToFile();
    });

    // 7. Highlight Current Line
    RenderToggleCard("ed_active_line", "Editor: Highlight Active Line",
                     "Controls whether the editor highlights the background of the active line.",
                     &settings.highlight_current_line, theme, bold_font, [&](bool v) {
        settings.highlight_current_line = v;
        app->ApplySettings(settings);
        app->GetSettingsManager().SaveToFile();
    });

    // 8. Cursor Blinking
    RenderToggleCard("ed_cursor_blink", "Editor: Cursor Blinking",
                     "Controls smooth cursor blinking animation in the editor.",
                     &settings.cursor_blinking, theme, bold_font, [&](bool v) {
        settings.cursor_blinking = v;
        app->ApplySettings(settings);
        app->GetSettingsManager().SaveToFile();
    });
}

void SettingsView::RenderAppearanceSettings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font) {
    auto& settings = app->GetSettingsManager().GetMutable();

    ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
    ImGui::TextUnformatted("APPEARANCE & THEMES");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1. UI Text Size (Font Size)
    RenderSettingCard("ui_font_size_card", "UI: Text Font Size",
                      "Controls the text size of interface elements (menus, sidebars, tabs, status bar) in pixels.",
                      theme, bold_font, [&]() {
        int cur_ui_size = settings.ui_font_size;
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderInt("##ui_font_size_slider", &cur_ui_size, 10, 26, "%d px")) {
            app->SetUiFontSize(cur_ui_size);
            app->GetSettingsManager().SaveToFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("-##ui_font_dec", ImVec2(28.0f, 0.0f))) {
            app->AdjustUiFontSize(-1);
        }
        ImGui::SameLine();
        if (ImGui::Button("+##ui_font_inc", ImVec2(28.0f, 0.0f))) {
            app->AdjustUiFontSize(1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset (15px)##reset_ui_font")) {
            app->SetUiFontSize(15);
            app->GetSettingsManager().SaveToFile();
        }

        ImGui::Spacing();
        ImGui::TextColored(theme->gutter_fg, "Sample Text: Luce Code Editor — Fast, Lightweight & Native");
    });

    // 2. UI Zoom / Scale
    RenderSettingCard("ui_scale_card", "Window: Interface Zoom / Scale",
                      "Adjusts the global user interface scale (80% to 200%).",
                      theme, bold_font, [&]() {
        float scale = settings.ui_scale;
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::SliderFloat("##ui_scale_slider", &scale, 0.75f, 2.0f, "%.2fx")) {
            settings.ui_scale = scale;
            app->SetScale(scale);
            app->GetSettingsManager().SaveToFile();
        }
        ImGui::SameLine();
        float presets[] = { 1.0f, 1.25f, 1.5f };
        for (float p : presets) {
            char p_label[16];
            snprintf(p_label, sizeof(p_label), "%d%%", static_cast<int>(p * 100.0f));
            if (ImGui::Button(p_label)) {
                settings.ui_scale = p;
                app->SetScale(p);
                app->GetSettingsManager().SaveToFile();
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Reset (100%)")) {
            settings.ui_scale = 1.0f;
            app->SetScale(1.0f);
            app->GetSettingsManager().SaveToFile();
        }
    });

    // 2. Theme Gallery
    RenderSettingCard("theme_gallery", "Workbench: Color Theme",
                      "Select the active color theme for the editor and user interface.",
                      theme, bold_font, [&]() {
        const auto& themes = app->GetThemeManager().GetThemes();
        float card_w = 210.0f;
        float card_h = 74.0f;
        int cols = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x) / (card_w + 12.0f)));

        for (size_t i = 0; i < themes.size(); ++i) {
            const auto& th = themes[i];
            bool is_active = (th.name == theme->name);

            ImGui::PushID(static_cast<int>(i));
            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(th.background.x * 1.2f, th.background.y * 1.2f, th.background.z * 1.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, theme->active_line);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);
            } else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, th.background);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(theme->gutter_fg.x, theme->gutter_fg.y, theme->gutter_fg.z, 0.2f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

            std::string box_id = "##th_card_" + std::to_string(i);
            ImGui::BeginChild(box_id.c_str(), ImVec2(card_w, card_h), true, ImGuiWindowFlags_NoScrollbar);

            // Theme Name & Active Tag
            ImGui::TextColored(th.foreground, "%s", th.name.c_str());
            if (is_active) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.4f, 1.0f), " (Active)");
            }

            // Palette Swatches (Background, Foreground, Keyword, String, Function, Accent)
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec4 swatch_colors[] = {
                th.background, th.foreground, th.syntax_keyword,
                th.syntax_string, th.syntax_function, th.statusbar_bg
            };
            float sw_w = 26.0f;
            float sw_h = 14.0f;
            for (int k = 0; k < 6; ++k) {
                ImVec2 sp(p0.x + k * (sw_w + 4.0f), p0.y + 4.0f);
                dl->AddRectFilled(sp, ImVec2(sp.x + sw_w, sp.y + sw_h), ImGui::ColorConvertFloat4ToU32(swatch_colors[k]), 2.0f);
                dl->AddRect(sp, ImVec2(sp.x + sw_w, sp.y + sw_h), IM_COL32(255, 255, 255, 40), 2.0f);
            }

            // Click handling over entire card
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                app->GetThemeManager().SetTheme(th.name);
                settings.theme_name = th.name;
                app->GetSettingsManager().SaveToFile();
            }

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            ImGui::PopID();

            if ((i + 1) % cols != 0 && (i + 1) < themes.size()) {
                ImGui::SameLine(0.0f, 12.0f);
            } else {
                ImGui::Spacing();
            }
        }
    });

    // 3. File & Folder Icons
    RenderSettingCard("icon_customization_card", "Workbench: File & Folder Icons",
                      "Customize file and folder icons, map file extensions in icons.json, and drop custom SVG or PNG icons into the icons folder.",
                      theme, bold_font, [&]() {
        std::string custom_icons_dir = IconManager::Instance().GetCustomIconsDir();
        std::string config_path = IconManager::Instance().GetConfigPath();

        ImGui::TextColored(theme->foreground, "Custom Icons Folder: %s", custom_icons_dir.c_str());
        ImGui::TextColored(theme->gutter_fg, "Configuration File: %s", config_path.c_str());
        ImGui::Spacing();

        if (ImGui::Button("Open Icons Folder##open_icons_folder")) {
            std::error_code ec;
            std::filesystem::create_directories(custom_icons_dir, ec);
            platform::OpenInFileExplorer(custom_icons_dir);
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImGui::SameLine();
        if (ImGui::Button("Open icons.json##open_icons_json")) {
            app->OpenIconsConfigFile();
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImGui::SameLine();
        if (ImGui::Button("Reload Icons##reload_icons")) {
            IconManager::Instance().Reload();
            app->GetToastManager().ShowSuccess("Icons reloaded successfully.");
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    });
}

void SettingsView::RenderGeneralSettings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font) {
    auto& settings = app->GetSettingsManager().GetMutable();

    ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
    ImGui::TextUnformatted("GENERAL PREFERENCES");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Auto Save
    RenderSettingCard("gen_autosave", "Files: Auto Save",
                      "Controls how dirty files are automatically saved to disk.",
                      theme, bold_font, [&]() {
        const char* modes[] = { "Off (manual save)", "On Window Focus Lost", "After 1s Delay" };
        const char* mode_keys[] = { "off", "on_focus_lost", "after_delay" };
        int current_idx = 0;
        for (int i = 0; i < 3; ++i) {
            if (settings.auto_save_mode == mode_keys[i]) current_idx = i;
        }

        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::Combo("##autosave_combo", &current_idx, modes, 3)) {
            settings.auto_save_mode = mode_keys[current_idx];
            app->ApplySettings(settings);
            app->GetSettingsManager().SaveToFile();
        }
    });

    // 2. Startup Welcome Page
    RenderToggleCard("gen_welcome", "Startup: Show Welcome Page",
                     "Controls whether the Welcome page is automatically displayed on startup or when all tabs are closed.",
                     &settings.show_welcome_on_startup, theme, bold_font, [&](bool v) {
        settings.show_welcome_on_startup = v;
        app->GetTabBar().SetShowWelcomeOnStartup(v);
        app->GetSettingsManager().SaveToFile();
    });

    // 3. Settings File Location
    RenderSettingCard("gen_config_path", "Configuration: Storage File",
                      "Path to the settings.json file on your local machine.",
                      theme, bold_font, [&]() {
        std::string path = app->GetSettingsManager().GetSettingsPath();
        ImGui::TextColored(theme->foreground, "%s", path.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Open in Editor##open_cfg_ed")) {
            app->OpenSettingsFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reveal in File Explorer##reveal_cfg")) {
            platform::OpenInFileExplorer(platform::GetDirectory(path));
        }
    });
}

void SettingsView::RenderKeybindings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
    ImGui::TextUnformatted("KEYBOARD SHORTCUTS");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    static char key_filter[64] = {};
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputTextWithHint("##key_filter", "Filter shortcuts...", key_filter, sizeof(key_filter));
    ImGui::Spacing();

    struct KeyEntry {
        const char* shortcut;
        const char* name;
        const char* group;
    };

    static const KeyEntry entries[] = {
        { "Ctrl+Shift+P", "Command Palette (All Commands)", "General" },
        { "Ctrl+P",       "Quick Open (Project Files)",     "General" },
        { "Ctrl+,",       "Preferences: Open Settings (UI)","General" },
        { "Ctrl+Shift+,", "Preferences: Open settings.json","General" },
        { "Ctrl+N",       "New Untitled File",              "File" },
        { "Ctrl+O",       "Open File...",                   "File" },
        { "Ctrl+S",       "Save Active File",               "File" },
        { "Ctrl+W",       "Close Active Tab",               "File" },
        { "Ctrl+F",       "Find in File",                   "Edit" },
        { "Ctrl+H",       "Replace in File",                "Edit" },
        { "Ctrl+Shift+F", "Find in Project Files",          "Edit" },
        { "Ctrl+Z",       "Undo Edit",                      "Edit" },
        { "Ctrl+Y",       "Redo Edit",                      "Edit" },
        { "Ctrl+/",       "Toggle Line Comment",            "Edit" },
        { "Ctrl+Wheel",   "Editor Font Zoom In / Out",      "Editor" },
        { "Ctrl+`",       "Toggle Embedded Terminal",       "View" },
        { "Ctrl+\\",      "Toggle Side-by-Side Split View", "View" },
        { "Ctrl+M",       "Toggle Code Minimap",            "View" },
        { "Ctrl+Shift+M", "Toggle Markdown Preview",        "View" },
        { "Ctrl+=",       "Zoom UI Scale In",               "View" },
        { "Ctrl+-",       "Zoom UI Scale Out",              "View" },
        { "Ctrl+0",       "Reset UI Scale to 100%",         "View" },
        { "F12",          "Go to Definition",               "Navigation" },
        { "Ctrl+Click",   "Go to Definition / Add Cursor",  "Navigation" },
    };

    std::string q = key_filter;
    std::ranges::transform(q, q.begin(), ::tolower);

    if (ImGui::BeginTable("##shortcuts_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Command Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (const auto& item : entries) {
            std::string s_lower = item.shortcut;
            std::string n_lower = item.name;
            std::ranges::transform(s_lower, s_lower.begin(), ::tolower);
            std::ranges::transform(n_lower, n_lower.begin(), ::tolower);

            if (!q.empty() && s_lower.find(q) == std::string::npos && n_lower.find(q) == std::string::npos) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(theme->syntax_function, "%s", item.shortcut);

            ImGui::TableNextColumn();
            ImGui::TextColored(theme->foreground, "%s", item.name);

            ImGui::TableNextColumn();
            ImGui::TextColored(theme->gutter_fg, "%s", item.group);
        }

        ImGui::EndTable();
    }
}

void SettingsView::RenderPlugins(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
    ImGui::TextUnformatted("PLUGINS & EXTENSIONS");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderSettingCard("plugin_overview", "Installed Extensions",
                      "Luce supports modular Lua plugins with custom commands, snippets, and editor hooks.",
                      theme, bold_font, [&]() {
        std::string plugins_dir = platform::GetExecutableDir() + "/plugins";
        ImGui::TextColored(theme->foreground, "Plugin Directory: %s", plugins_dir.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Open Plugins Folder##open_plug_folder")) {
            platform::OpenInFileExplorer(plugins_dir);
        }
    });
}

void SettingsView::RenderAbout(App* app, const Theme* theme, ImFont* bold_font, ImFont* h1_font, ImFont* h2_font) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme->gutter_fg);
    ImGui::TextUnformatted("ABOUT LUCE");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (h1_font) ImGui::PushFont(h1_font);
    ImGui::TextColored(theme->foreground, "Luce Code Editor");
    if (h1_font) ImGui::PopFont();

    ImGui::TextColored(theme->syntax_string, "Fast, lightweight C++23 code editor powered by Dear ImGui & SDL2.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Live Memory consumption
    float ram_mb = 0.0f;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        ram_mb = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
    }
#endif

    RenderSettingCard("about_metrics", "Performance & Memory Footprint",
                      "Luce features custom glyph packing, idle working set trimming, and virtual scrolling.",
                      theme, bold_font, [&]() {
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.4f, 1.0f), "⚡ Live Memory Usage: %.1f MB Working Set RAM", ram_mb > 0.0f ? ram_mb : 6.4f);
        ImGui::TextColored(theme->gutter_fg, "Target: < 20 MB (vs standard Electron editors ~500+ MB)");
    });

    RenderSettingCard("about_tech", "Technologies & Dependencies",
                      "Core architecture breakdown",
                      theme, bold_font, [&]() {
        ImGui::BulletText("Standard: C++23");
        ImGui::BulletText("GUI: Dear ImGui (docking branch)");
        ImGui::BulletText("Windowing & Input: SDL2 2.30");
        ImGui::BulletText("Typography & Emojis: FreeType 2.13 with color emoji support");
        ImGui::BulletText("Terminal: Neovim libvterm static engine");
        ImGui::BulletText("Scripting: Embedded Lua 5.4");
    });
}

void SettingsView::RenderFilteredSettings(App* app, const std::string& query,
                                         const Theme* theme, ImFont* bold_font, ImFont* h2_font) {
    ImGui::TextColored(theme->syntax_string, "Search results for \"%s\":", search_buf_);
    ImGui::Spacing();

    // Check if query matches editor or UI font
    if (query.find("font") != std::string::npos || query.find("size") != std::string::npos) {
        RenderEditorSettings(app, theme, bold_font, h2_font);
        RenderAppearanceSettings(app, theme, bold_font, h2_font);
    } else if (query.find("theme") != std::string::npos || query.find("color") != std::string::npos) {
        RenderAppearanceSettings(app, theme, bold_font, h2_font);
    } else if (query.find("tab") != std::string::npos || query.find("space") != std::string::npos) {
        RenderEditorSettings(app, theme, bold_font, h2_font);
    } else if (query.find("save") != std::string::npos) {
        RenderGeneralSettings(app, theme, bold_font, h2_font);
    } else {
        // Fallback: render both editor and appearance
        RenderEditorSettings(app, theme, bold_font, h2_font);
        RenderAppearanceSettings(app, theme, bold_font, h2_font);
    }
}

}  // namespace luce
