#pragma once
// ============================================================================
// SettingsManager — Manages persistent editor settings in settings.json (Zed-style).
// ============================================================================

#include <string>
#include <functional>

namespace luce {

struct AppSettings {
    // Editor settings
    int   editor_font_size        = 15;    // 10 to 36 px (base font is 15.0f)
    int   tab_size                = 4;     // 2, 4, 8
    bool  use_spaces              = true;  // Insert spaces instead of tabs
    bool  show_minimap            = true;  // Show code minimap
    bool  show_line_numbers       = true;  // Show line numbers in gutter
    bool  highlight_current_line  = true;  // Highlight active line
    bool  zoom_with_mouse_wheel   = true;  // Zoom font with Ctrl + MouseWheel
    bool  cursor_blinking         = true;  // Cursor blinking animation

    // UI & Appearance settings
    int   ui_font_size            = 15;    // 10 to 28 px (base UI font is 15.0f)
    float ui_scale                = 1.0f;  // 0.75f to 2.5f
    std::string theme_name        = "VS Code Dark 2026";
    std::string auto_save_mode    = "off"; // "off", "on_focus_lost", "after_delay"
    bool  show_welcome_on_startup = true;  // Show Welcome Screen on first launch or startup

    // History
    std::vector<std::string> recent_projects; // List of recently opened folders
};

class SettingsManager {
public:
    SettingsManager();

    /// Get path to settings.json
    std::string GetSettingsPath() const;

    /// Ensure settings.json exists on disk with default formatted contents.
    bool EnsureDefaultSettingsFile();

    /// Load settings from settings.json. Returns true on success.
    bool LoadFromFile(const std::string& path = "");

    /// Save current settings to settings.json. Returns true on success.
    bool SaveToFile(const std::string& path = "");

    /// Add a folder path to recent projects list (deduplicates, pushes to front, saves).
    void AddRecentProject(const std::string& path);

    const AppSettings& Get() const { return settings_; }
    AppSettings& GetMutable() { return settings_; }
    void Set(const AppSettings& s) { settings_ = s; }

private:
    AppSettings settings_;
};

}  // namespace luce
