#pragma once
// ============================================================================
// WelcomeView — Welcome / Getting Started screen for Luce editor (Zed-style).
// ============================================================================

#include "ui/theme.h"
#include "imgui.h"
#include <string>

namespace luce {

class App;

class WelcomeView {
public:
    WelcomeView();

    /// Render the Welcome View within the active editor tab area.
    void Render(App* app, const Theme* theme, ImFont* bold_font = nullptr,
                ImFont* italic_font = nullptr, ImFont* h1_font = nullptr,
                ImFont* h2_font = nullptr);

private:
    /// Helper to render an action row styled identically to Luce explorer/git/plugins
    bool RenderActionRow(const char* id, ImTextureID icon, const char* title,
                         const char* shortcut, const Theme* theme, float width,
                         ImFont* bold_font = nullptr);

    /// Helper to render a recent workspace row
    bool RenderRecentRow(const char* id, const std::string& folder_path,
                         const Theme* theme, float width, ImFont* bold_font = nullptr);

    /// Section header matching Explorer/Git uppercase style
    void RenderSectionHeader(const char* title, const Theme* theme, ImFont* bold_font = nullptr);
};

} // namespace luce
