#pragma once
// ============================================================================
// App — Main application shell that owns all UI components and orchestrates
// the docking layout, menu bar, status bar, and global keybindings.
// ============================================================================

#include "ui/command_palette.h"
#include "ui/file_explorer.h"
#include "ui/tab_bar.h"
#include "ui/terminal_panel.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"
#include "plugin/plugin_manager.h"

#include <string>
#include <memory>

namespace luce {

class App {
public:
    App();
    ~App();

    /// Called once per frame from the main loop.
    void Render();

    /// Handle a file dropped onto the window.
    void OnFileDrop(const std::string& path);

    /// Whether the app wants to quit (e.g. user clicked close).
    bool WantsQuit() const { return wants_quit_; }

    /// Provide loaded fonts to the app
    void SetFonts(ImFont* regular, ImFont* editor_mono, ImFont* bold, ImFont* italic, ImFont* h1, ImFont* h2) {
        font_regular_ = regular;
        font_editor_  = editor_mono;
        font_bold_    = bold;
        font_italic_  = italic;
        font_h1_      = h1;
        font_h2_      = h2;
    }

    void ZoomIn()   { SetScale(ui_scale_ + 0.1f); }
    void ZoomOut()  { SetScale(ui_scale_ - 0.1f); }
    void ResetZoom(){ SetScale(1.0f); }
    void SetScale(float s) {
        ui_scale_ = std::clamp(s, 0.5f, 2.5f);
        ImGui::GetIO().FontGlobalScale = ui_scale_;
    }

    /// Get the current theme background color
    ImVec4 GetBackgroundColor() const;

    TabBar& GetTabBar() { return tab_bar_; }
    ThemeManager& GetThemeManager() { return theme_manager_; }
    CommandPalette& GetCommandPalette() { return command_palette_; }
    FileExplorer& GetFileExplorer() { return file_explorer_; }
    class ToastManager& GetToastManager() { return toast_manager_; }
    void TriggerSaveSession() { SaveSession(); }

private:
    // ── Rendering helpers ─────────────────────────────────────────────────
    void RenderMenuBar();
    void RenderStatusBar();
    void RenderSourceControl();
    void SetupDockspace();

    // ── Session persistence ──────────────────────────────────────────────
    void LoadSession();
    void SaveSession();

    // ── Command registration ──────────────────────────────────────────────
    void RegisterCommands();

    // ── Project file scanning (for Quick Open) ────────────────────────────
    void ScanProjectFiles();

    // ── State ─────────────────────────────────────────────────────────────
    ThemeManager      theme_manager_;
    TabBar            tab_bar_;
    FileExplorer      file_explorer_;
    CommandPalette    command_palette_;
    TerminalPanel     terminal_;
    ToastManager      toast_manager_;
    std::unique_ptr<class PluginManager> plugin_manager_;

    ImFont*           font_regular_ = nullptr; // IBM Plex Sans UI
    ImFont*           font_editor_  = nullptr; // Lilex Monospace Code
    ImFont*           font_bold_    = nullptr; // IBM Plex Sans Bold
    ImFont*           font_italic_  = nullptr; // IBM Plex Sans Italic
    ImFont*           font_h1_      = nullptr;
    ImFont*           font_h2_      = nullptr;

    float             ui_scale_           = 1.0f;
    bool              wants_quit_         = false;
    bool              show_file_explorer_ = true;
    bool              show_source_control_= false;
    bool              show_terminal_      = true;
    bool              show_plugins_       = false;
    bool              show_demo_window_   = false;
    bool              show_about_modal_   = false;
};

}  // namespace luce
