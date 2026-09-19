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
#include "ui/settings_manager.h"
#include "ui/settings_view.h"
#include "ui/welcome_view.h"
#include "plugin/plugin_manager.h"
#include "editor/symbol_index.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace luce {

class App {
public:
    App();
    ~App();

    /// Called once per frame from the main loop.
    void Render();

    /// Handle a file dropped onto the window.
    void OnFileDrop(const std::string& path);

    /// Handle application window gaining focus.
    void OnFocusGained();

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
        if (font_editor_) {
            font_editor_->Scale = static_cast<float>(settings_manager_.Get().editor_font_size) / 15.0f;
        }
        SetUiFontSize(settings_manager_.Get().ui_font_size);
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
    SymbolIndex& GetSymbolIndex() { return symbol_index_; }
    void SaveSession();
    void TriggerSaveSession() { SaveSession(); }

    bool IsMinimapEnabled() const { return show_minimap_; }
    void SetMinimapEnabled(bool enabled);
    void ToggleMinimap() { SetMinimapEnabled(!show_minimap_); }

    SettingsManager& GetSettingsManager() { return settings_manager_; }
    const SettingsManager& GetSettingsManager() const { return settings_manager_; }
    SettingsView& GetSettingsView() { return settings_view_; }

    void OpenSettingsFile();
    void OpenIconsConfigFile();
    void OpenSettingsTab();
    void OpenSettingsWindow();
    void CloseSettingsWindow();
    bool IsSettingsWindowOpen() const { return show_settings_window_; }
    void OpenWelcomeTab();
    void OpenWorkspaceFolder(const std::string& folder);
    void ShowGitCloneModal() { show_git_clone_modal_ = true; }
    void ShowPluginsPanel() { show_plugins_ = true; show_file_explorer_ = false; show_source_control_ = false; }
    WelcomeView& GetWelcomeView() { return welcome_view_; }
    void ApplySettings(const AppSettings& s);
    void SetEditorFontSize(int size);
    void AdjustEditorFontSize(int delta);
    void SetUiFontSize(int size);
    void AdjustUiFontSize(int delta);
    ImFont* GetEditorFont() const { return font_editor_; }

    struct ConfirmationModal {
        bool request_open = false;
        std::string title = "Confirm Action";
        std::string message;
        std::string confirm_label = "Confirm";
        ImVec4 confirm_color = ImVec4(0.85f, 0.25f, 0.25f, 1.0f);
        std::function<void()> on_confirm;
    };

    void RequestConfirmation(const std::string& title,
                             const std::string& message,
                             const std::string& confirm_label,
                             const ImVec4& confirm_color,
                             std::function<void()> on_confirm) {
        confirm_modal_.title = title;
        confirm_modal_.message = message;
        confirm_modal_.confirm_label = confirm_label;
        confirm_modal_.confirm_color = confirm_color;
        confirm_modal_.on_confirm = std::move(on_confirm);
        confirm_modal_.request_open = true;
    }

private:
    // ── Rendering helpers ─────────────────────────────────────────────────
    void RenderMenuBar();
    void RenderStatusBar();
    void RenderSourceControl();
    void RenderPluginsPanel();
    void RenderPluginIcon(const LuaPluginInfo& info, float size);
    void RenderGitModals();
    void ShowGitDiffModal(const std::string& path);
    void RenderGitDiffModal();
    void RenderSettingsWindow();
    void SetupDockspace();

    // ── Session persistence ──────────────────────────────────────────────
    void LoadSession();

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
    SettingsManager   settings_manager_;
    SettingsView      settings_view_;
    WelcomeView       welcome_view_;
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
    int               selected_plugin_index_ = -1;
    bool              show_demo_window_   = false;
    bool              show_about_modal_   = false;
    bool              show_git_branch_modal_ = false;
    bool              show_git_remote_modal_ = false;
    bool              show_git_stash_modal_  = false;
    bool              show_git_tags_modal_    = false;
    bool              show_git_clone_modal_   = false;
    bool              show_git_output_modal_  = false;
    bool              show_git_diff_modal_    = false;
    std::string       git_diff_file_;
    std::string       git_diff_content_;
    bool              git_view_as_tree_       = false;
    bool              show_minimap_           = true;
    bool              show_settings_window_   = false;

    SymbolIndex       symbol_index_;
    ConfirmationModal confirm_modal_;

    std::thread           project_scan_thread_;
    std::atomic<uint32_t> project_scan_generation_{0};
};

}  // namespace luce
