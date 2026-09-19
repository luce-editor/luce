#pragma once
// ============================================================================
// SettingsView — Full-featured, modern Settings UI tab for Luce (Zed/VS Code style).
// ============================================================================

#include "ui/theme.h"
#include <string>
#include <functional>

struct ImFont;

namespace luce {

class App;

enum class SettingsCategory {
    Editor,
    Appearance,
    General,
    Keybindings,
    Plugins,
    About
};

class SettingsView {
public:
    SettingsView();

    void Render(App* app, const Theme* theme, ImFont* bold_font,
                ImFont* italic_font, ImFont* h1_font, ImFont* h2_font);

    void SetCategory(SettingsCategory cat) { active_category_ = cat; }
    SettingsCategory GetCategory() const { return active_category_; }

private:
    SettingsCategory active_category_ = SettingsCategory::Editor;
    char search_buf_[128] = {};

    void RenderHeader(App* app, const Theme* theme, ImFont* bold_font, ImFont* h1_font);
    void RenderSidebar(const Theme* theme, ImFont* bold_font);
    void RenderCategoryContent(App* app, const Theme* theme, ImFont* bold_font, ImFont* italic_font, ImFont* h1_font, ImFont* h2_font);

    void RenderEditorSettings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font);
    void RenderAppearanceSettings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font);
    void RenderGeneralSettings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font);
    void RenderKeybindings(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font);
    void RenderPlugins(App* app, const Theme* theme, ImFont* bold_font, ImFont* h2_font);
    void RenderAbout(App* app, const Theme* theme, ImFont* bold_font, ImFont* h1_font, ImFont* h2_font);

    void RenderFilteredSettings(App* app, const std::string& query, const Theme* theme, ImFont* bold_font, ImFont* h2_font);

    void RenderSettingCard(const char* id, const char* title, const char* description,
                           const Theme* theme, ImFont* bold_font, std::function<void()> draw_control);
    void RenderToggleCard(const char* id, const char* title, const char* description,
                          bool* val, const Theme* theme, ImFont* bold_font,
                          std::function<void(bool)> on_change);
};

/// Render a modern pill-shaped toggle switch (slider) in Zed/iOS style.
bool ToggleSwitch(const char* str_id, bool* v, const Theme* theme = nullptr, float custom_height = 22.0f);

}  // namespace luce
