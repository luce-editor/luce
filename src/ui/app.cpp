// ============================================================================
// App — Implementation.
//
// Sets up the ImGui docking layout, renders the menu bar, status bar,
// file explorer, editor tabs, terminal, and command palette.  Registers
// all built-in commands and global keyboard shortcuts.
// ============================================================================

#include "app.h"
#include "ui/icon_manager.h"
#include "platform.h"
#include "external/json.hpp"
#include "../editor/diagnostic.h"
#include "../editor/git_manager.h"
#include "../editor/diagnostic_runner.h"
#include "imgui.h"
#include "imgui_internal.h"  // For DockBuilder API.

#include <algorithm>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace luce {

App::App() {
    // Apply the default theme to ImGui.
    theme_manager_.ApplyToImGui();

    // Initialize Plugin Manager
    plugin_manager_ = std::make_unique<PluginManager>(this);
    std::string exe_dir = platform::GetExecutableDir();
    plugin_manager_->Init(exe_dir + "/plugins");

    // Wire up the file explorer to open files in the tab bar.
    file_explorer_.SetOnOpenFile([this](const std::string& path) {
        tab_bar_.OpenFile(path, &theme_manager_.Active());
        SaveSession();
    });
    file_explorer_.SetOnOpenFolder([this]() {
        std::string folder = platform::OpenFolderDialog();
        if (!folder.empty()) {
            file_explorer_.SetRoot(folder);
            terminal_.SetWorkingDirectory(folder);
            GitManager::Instance().SetRepoPath(folder);
            ScanProjectFiles();
            SaveSession();
        }
    });
    file_explorer_.SetOnRemoveFolder([this]() {
        file_explorer_.SetRoot("");
        GitManager::Instance().SetRepoPath("");
        command_palette_.SetProjectFiles({});
        SaveSession();
    });

    // Wire up the command palette.
    command_palette_.SetOnOpenFile([this](const std::string& path) {
        tab_bar_.OpenFile(path, &theme_manager_.Active());
        SaveSession();
    });
    command_palette_.SetOnGoToLine([this](int line) {
        if (auto* editor = tab_bar_.ActiveEditor()) {
            editor->GoToLine(line);
        }
    });

    // Restore previous folder and open files FIRST so explorer root is known
    LoadSession();

    // Sync Git repository with restored root
    GitManager::Instance().SetRepoPath(file_explorer_.Root());

    // Start the terminal shell.
    terminal_.StartShell();

    // Register built-in commands.
    RegisterCommands();

    // Create an initial untitled tab only if no tabs were restored.
    if (tab_bar_.TabCount() == 0) {
        tab_bar_.NewFile(&theme_manager_.Active());
    }
}

App::~App() = default;

ImVec4 App::GetBackgroundColor() const {
    return theme_manager_.Active().background;
}

// ── Main render ───────────────────────────────────────────────────────────

void App::Render() {
    // Apply theme (in case it was changed via command palette).
    theme_manager_.ApplyToImGui();

    // Full-viewport dockspace.
    SetupDockspace();

    // Menu bar.
    RenderMenuBar();

    // Sidebar panel (contains horizontal activity bar + active view).
    bool show_sidebar = show_file_explorer_ || show_source_control_ || show_plugins_;
    if (show_sidebar) {
        ImGuiWindowClass window_class;
        window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
        ImGui::SetNextWindowClass(&window_class);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Sidebar", &show_sidebar, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
        ImGui::PopStyleVar();

        // Top horizontal icon bar
        float icon_size = 20.0f * ui_scale_;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
        
        ImVec2 cursor_start = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursor_start.x + 8.0f * ui_scale_, cursor_start.y + 4.0f * ui_scale_));

        // Explorer Button
        ImTextureID explorer_icon = IconManager::Instance().GetIconByName("default_folder");
        if (explorer_icon) {
            if (ImGui::ImageButton("##horiz_explorer", explorer_icon, ImVec2(icon_size, icon_size))) {
                show_file_explorer_ = true;
                show_source_control_ = false;
                show_plugins_ = false;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Explorer");
        }
        
        ImGui::SameLine();

        // Source Control Button (Git)
        ImTextureID git_icon = IconManager::Instance().GetIconByName("file_type_git");
        if (git_icon) {
            if (ImGui::ImageButton("##horiz_git", git_icon, ImVec2(icon_size, icon_size))) {
                show_source_control_ = true;
                show_file_explorer_ = false;
                show_plugins_ = false;
                GitManager::Instance().Refresh();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Source Control (Git)");
        }

        ImGui::SameLine();
        
        // Plugins Button
        ImTextureID plugin_icon = IconManager::Instance().GetIconByName("folder_type_plugin");
        if (plugin_icon) {
            if (ImGui::ImageButton("##horiz_plugins", plugin_icon, ImVec2(icon_size, icon_size))) {
                show_plugins_ = true;
                show_file_explorer_ = false;
                show_source_control_ = false;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Plugins");
        }
        ImGui::PopStyleColor(3);

        // Active indicator line
        ImVec2 draw_pos = ImGui::GetCursorScreenPos();
        draw_pos.y -= 2.0f; // Shift slightly up
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        float indicator_width = icon_size + 8.0f; // approximate button width
        float start_x = draw_pos.x + 8.0f * ui_scale_;
        if (show_source_control_) start_x += indicator_width + 4.0f; // offset for second button
        else if (show_plugins_) start_x += (indicator_width + 4.0f) * 2.0f; // offset for third button

        draw_list->AddLine(ImVec2(start_x, draw_pos.y), ImVec2(start_x + indicator_width, draw_pos.y), 
                           IM_COL32(0, 122, 204, 255), 2.0f); // Blue VSCode color

        ImGui::Spacing();
        ImGui::Separator();

        // Render active content
        if (show_file_explorer_) {
            file_explorer_.Render();
        } else if (show_source_control_) {
            RenderSourceControl();
        } else if (show_plugins_) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "INSTALLED PLUGINS");

            float icon_sz = 14.0f * ui_scale_;
            float btn_w = icon_sz + ImGui::GetStyle().FramePadding.x * 2.0f;
            float right_x = ImGui::GetWindowContentRegionMax().x - btn_w - 4.0f;
            if (right_x > ImGui::GetCursorPosX()) {
                ImGui::SameLine(right_x);
            } else {
                ImGui::SameLine();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));

            ImTextureID refresh_icon = IconManager::Instance().GetIconByName("refresh");
            if (refresh_icon) {
                if (ImGui::ImageButton("##plugins_reload_top", refresh_icon, ImVec2(icon_sz, icon_sz))) {
                    plugin_manager_->ReloadPlugins();
                    toast_manager_.ShowSuccess("Plugins: Reloaded all plugins.");
                }
            } else {
                if (ImGui::SmallButton("↻##plugins_reload_top")) {
                    plugin_manager_->ReloadPlugins();
                    toast_manager_.ShowSuccess("Plugins: Reloaded all plugins.");
                }
            }
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reload Plugins");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            const auto& plugins = plugin_manager_->GetLoadedPlugins();
            if (plugins.empty()) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
                ImGui::TextDisabled("No Lua plugins installed.");
                ImGui::Spacing();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
                ImGui::TextWrapped("Drop .lua files into the plugins/ folder next to luce.exe and click Reload.");
            } else {
                std::optional<size_t> plugin_to_uninstall;
                for (size_t i = 0; i < plugins.size(); ++i) {
                    const auto& p = plugins[i];
                    const auto& info = p->GetInfo();
                    ImGui::PushID((int)i);
                    
                    // Plugin icon
                    ImTextureID item_icon = IconManager::Instance().GetIconByName("folder_type_plugin");
                    if (item_icon) {
                        ImGui::Image(item_icon, ImVec2(40, 40));
                    } else {
                        ImGui::ColorButton("##icon", ImVec4(0.4f, 0.2f, 0.8f, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(40, 40));
                    }
                    ImGui::SameLine();
                    
                    ImGui::BeginGroup();
                    
                    float top_y = ImGui::GetCursorPosY();

                    // Name and version
                    if (font_bold_) ImGui::PushFont(font_bold_);
                    ImGui::Text("%s", info.name.c_str());
                    if (font_bold_) ImGui::PopFont();
                    
                    ImGui::SameLine();
                    ImGui::TextDisabled("v%s", info.version.c_str());
                    
                    // Uninstall icon right aligned
                    ImTextureID delete_icon = IconManager::Instance().GetIconByName("delete");
                    if (delete_icon) {
                        float icon_size_px = 16.0f;
                        float btn_w = icon_size_px + ImGui::GetStyle().FramePadding.x * 2.0f;
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - btn_w);
                        ImGui::SetCursorPosY(top_y - ImGui::GetStyle().FramePadding.y);
                        
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 0.3f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.2f, 0.2f, 0.5f));
                        if (ImGui::ImageButton("##uninstall", delete_icon, ImVec2(icon_size_px, icon_size_px))) {
                            plugin_to_uninstall = i;
                        }
                        ImGui::PopStyleColor(3);
                    } else {
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65.0f);
                        if (ImGui::Button("Unload")) {
                            plugin_to_uninstall = i;
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Unload plugin (does not delete the .lua file)");
                    
                    // Author
                    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", info.author.c_str());
                    
                    // Description
                    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x - 8.0f);
                    ImGui::TextWrapped("%s", info.description.empty() ? "No description provided." : info.description.c_str());
                    ImGui::PopTextWrapPos();
                    
                    ImGui::EndGroup();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::PopID();
                }
                
                if (plugin_to_uninstall) {
                    plugin_manager_->UninstallPlugin(*plugin_to_uninstall, false);
                }
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
            if (ImGui::Button("Reload Plugins")) {
                plugin_manager_->ReloadPlugins();
                toast_manager_.ShowSuccess("Plugins: Reloaded all plugins.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Open Plugins Folder")) {
                platform::OpenInFileExplorer(platform::GetExecutableDir() + "/plugins");
            }
        }

        ImGui::End();
        
        // Handle window close
        if (!show_sidebar) {
            show_file_explorer_ = false;
            show_source_control_ = false;
            show_plugins_ = false;
        }
    }

    // Editor panel (tabs + code).
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
    tab_bar_.Render(&theme_manager_.Active(), font_editor_, font_bold_, font_italic_, font_h1_, font_h2_);
    ImGui::End();

    // Bottom panel.
    if (show_terminal_) {
        ImGuiWindowClass window_class;
        window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
        ImGui::SetNextWindowClass(&window_class);
        
        ImGui::Begin("Panel", &show_terminal_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
        
        if (ImGui::BeginTabBar("##bottom_panel_tabs")) {
            if (ImGui::BeginTabItem("Problems")) {
                const auto& diags = DiagnosticManager::Instance().GetDiagnostics();
                if (diags.empty()) {
                    ImGui::TextDisabled("No problems have been detected in the workspace.");
                } else {
                    if (ImGui::BeginTable("##diagnostics", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                        ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableHeadersRow();

                        for (size_t i = 0; i < diags.size(); ++i) {
                            const auto& diag = diags[i];
                            ImGui::TableNextRow();
                            
                            ImGui::TableNextColumn();
                            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                            if (diag.severity == DiagnosticSeverity::Error) color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                            else if (diag.severity == DiagnosticSeverity::Warning) color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
                            
                            ImGui::PushStyleColor(ImGuiCol_Text, color);
                            
                            // Make the row selectable
                            char label[32];
                            snprintf(label, sizeof(label), "##diag_%zu", i);
                            if (ImGui::Selectable(label, false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                                tab_bar_.OpenFile(diag.file_path, &theme_manager_.Active());
                                if (auto* editor = tab_bar_.ActiveEditor()) {
                                    editor->GoToLine(diag.line);
                                }
                            }
                            
                            ImGui::SameLine();
                            ImGui::TextUnformatted(diag.message.c_str());
                            ImGui::PopStyleColor();
                            
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(diag.file_path.c_str());
                            
                            ImGui::TableNextColumn();
                            ImGui::Text("%d:%d", diag.line, diag.column);
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Output")) {
                ImGui::TextDisabled("Output will appear here.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Terminal")) {
                if (font_editor_) ImGui::PushFont(font_editor_);
                terminal_.Render(theme_manager_.Active());
                if (font_editor_) ImGui::PopFont();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        
        ImGui::End();
    }

    // Status bar.
    RenderStatusBar();

    // Per-frame plugin tick.
    if (plugin_manager_) {
        plugin_manager_->Tick(ImGui::GetIO().DeltaTime);
    }

    // Command palette overlay.
    command_palette_.Render();

    // Floating toast notifications (VS Code style).
    toast_manager_.Render(theme_manager_.Active());

    // ImGui demo window (for debugging, toggled from menu).
    if (show_demo_window_) ImGui::ShowDemoWindow(&show_demo_window_);

    // ── Global keyboard shortcuts ─────────────────────────────────────────
    ImGuiIO& io = ImGui::GetIO();

    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P))
        command_palette_.Open(PaletteMode::Commands);

    if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P))
        command_palette_.Open(PaletteMode::Files);

    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_M))
        tab_bar_.ToggleActiveMarkdownPreview();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G))
        command_palette_.Open(PaletteMode::GoToLine);

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))
        tab_bar_.NewFile(&theme_manager_.Active());

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        std::string path = platform::OpenFileDialog();
        if (!path.empty()) tab_bar_.OpenFile(path, &theme_manager_.Active());
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        tab_bar_.SaveActive();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W))
        tab_bar_.CloseTab(tab_bar_.ActiveIndex());

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Tab))
        tab_bar_.NextTab();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_GraveAccent))
        show_terminal_ = !show_terminal_;

    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_T))
        terminal_.NewTerminal();

    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_W))
        terminal_.CloseCurrentTerminal();

    // GUI Zoom / Scale shortcuts (Ctrl+ / Ctrl- / Ctrl+0)
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))) {
        ZoomIn();
        SaveSession();
    }
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))) {
        ZoomOut();
        SaveSession();
    }
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsKeyPressed(ImGuiKey_Keypad0))) {
        ResetZoom();
        SaveSession();
    }
}

// ── Dockspace ─────────────────────────────────────────────────────────────

/// Create a full-viewport dockspace.  On the first frame, build a default
/// layout (file explorer left, editor center, terminal bottom).
void App::SetupDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Reserve room at the bottom for the status bar, which is a separate
    // floating window drawn over the viewport.  Without this the dockspace
    // (and therefore the bottom-docked terminal's input field) extends
    // underneath the status bar, which covers it and swallows its clicks —
    // making the terminal input impossible to focus or type into.
    float status_bar_height = ImGui::GetFrameHeight() + 4.0f;
    ImVec2 dock_pos  = viewport->WorkPos;
    ImVec2 dock_size = ImVec2(viewport->WorkSize.x,
                              viewport->WorkSize.y - status_bar_height);

    ImGui::SetNextWindowPos(dock_pos);
    ImGui::SetNextWindowSize(dock_size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##dockspace_root", nullptr, flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("LuceDockSpace");

    // Build the default layout on the first frame.
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoWindowMenuButton);
        ImGui::DockBuilderSetNodeSize(dockspace_id, dock_size);

        ImGuiID dock_main     = dockspace_id;
        ImGuiID dock_left     = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left,   0.20f, nullptr, &dock_main);
        ImGuiID dock_bottom   = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down,   0.25f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Sidebar", dock_left);
        ImGui::DockBuilderDockWindow("Editor",   dock_main);
        ImGui::DockBuilderDockWindow("Panel",  dock_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_NoWindowMenuButton);
    ImGui::End();
}

// ── Menu bar ──────────────────────────────────────────────────────────────

void App::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New File",        "Ctrl+N"))        tab_bar_.NewFile(&theme_manager_.Active());
            if (ImGui::MenuItem("Open File...",    "Ctrl+O"))        {
                std::string p = platform::OpenFileDialog();
                if (!p.empty()) {
                    tab_bar_.OpenFile(p, &theme_manager_.Active());
                    SaveSession();
                }
            }
            if (ImGui::MenuItem("Open Folder...")) {
                std::string folder = platform::OpenFolderDialog();
                if (!folder.empty()) {
                    file_explorer_.SetRoot(folder);
                    ScanProjectFiles();
                    SaveSession();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save",            "Ctrl+S"))        tab_bar_.SaveActive();
            if (ImGui::MenuItem("Save As...")) {
                std::string p = platform::SaveFileDialog();
                if (!p.empty()) {
                    tab_bar_.SaveActiveAs(p);
                    SaveSession();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close Editor",    "Ctrl+W")) {
                tab_bar_.CloseTab(tab_bar_.ActiveIndex());
                SaveSession();
            }
            if (ImGui::MenuItem("Close Project")) {
                file_explorer_.SetRoot("");
                command_palette_.SetProjectFiles({});
                SaveSession();
            }
            if (ImGui::MenuItem("Close Window",    "Alt+F4"))        wants_quit_ = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo",   "Ctrl+Z"))  { if (auto* e = tab_bar_.ActiveEditor()) e->Undo(); }
            if (ImGui::MenuItem("Redo",   "Ctrl+Y"))  { if (auto* e = tab_bar_.ActiveEditor()) e->Redo(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut",    "Ctrl+X"))  { if (auto* e = tab_bar_.ActiveEditor()) e->Cut(); }
            if (ImGui::MenuItem("Copy",   "Ctrl+C"))  { if (auto* e = tab_bar_.ActiveEditor()) e->Copy(); }
            if (ImGui::MenuItem("Paste",  "Ctrl+V"))  { if (auto* e = tab_bar_.ActiveEditor()) e->Paste(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Find",    "Ctrl+F"))  { if (auto* e = tab_bar_.ActiveEditor()) e->OpenFind(); }
            if (ImGui::MenuItem("Replace", "Ctrl+H"))  { if (auto* e = tab_bar_.ActiveEditor()) e->OpenReplace(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Explorer",   nullptr, &show_file_explorer_);
            ImGui::MenuItem("Plugins",    nullptr, &show_plugins_);
            ImGui::MenuItem("Terminal",   "Ctrl+`", &show_terminal_);
            if (ImGui::MenuItem("New Terminal", "Ctrl+Shift+T")) {
                terminal_.NewTerminal();
            }
            if (ImGui::MenuItem("Close Terminal", "Ctrl+Shift+W")) {
                terminal_.CloseCurrentTerminal();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                for (auto& name : theme_manager_.GetThemeNames()) {
                    bool selected = (name == theme_manager_.Active().name);
                    if (ImGui::MenuItem(name.c_str(), nullptr, selected)) {
                        theme_manager_.SetTheme(name);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reload Themes")) {
                    theme_manager_.ReloadThemes();
                }
                if (ImGui::MenuItem("Open Themes Folder...")) {
                    std::string themes_dir = platform::GetExecutableDir() + "/themes";
                    std::error_code ec;
                    fs::create_directories(themes_dir, ec);
                    platform::OpenInFileExplorer(themes_dir);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_window_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Command Palette...", "Ctrl+Shift+P"))
                command_palette_.Open(PaletteMode::Commands);
            if (ImGui::MenuItem("Quick Open...",      "Ctrl+P"))
                command_palette_.Open(PaletteMode::Files);
            if (ImGui::MenuItem("Go to Line...",      "Ctrl+G"))
                command_palette_.Open(PaletteMode::GoToLine);
            ImGui::Separator();
            if (ImGui::MenuItem("Reload Plugins")) {
                if (plugin_manager_) {
                    plugin_manager_->ReloadPlugins();
                    toast_manager_.ShowSuccess("Plugins: Reloaded all plugins.");
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Luce")) {
                show_about_modal_ = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (show_about_modal_) {
        ImGui::OpenPopup("About Luce##modal");
        show_about_modal_ = false;
    }
    if (ImGui::BeginPopupModal("About Luce##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.0f, 0.47f, 0.83f, 1.0f), "Luce Code Editor");
        ImGui::Text("Version: %s", LUCE_VERSION);
        ImGui::Separator();
        ImGui::Text("A modern, fast code editor written in C++23 using Dear ImGui, SDL2, and OpenGL3.");
        ImGui::Spacing();
        ImGui::Text("Key Features:");
        ImGui::BulletText("Full syntax highlighting (C++, Rust, Web, Markdown, CMake)");
        ImGui::BulletText("Direct SVG vector icon rasterizer");
        ImGui::BulletText("Live Markdown Preview (Ctrl+Shift+M)");
        ImGui::BulletText("Embedded Terminal & Command Palette (Ctrl+Shift+P)");
        ImGui::BulletText("Emmet HTML Snippets (Tab expansion)");
        ImGui::BulletText("Lua 5.4 scripting plugin system (zero compilation, drop .lua files)");
        ImGui::BulletText("Session state persistence (JSON)");
        ImGui::Spacing();

        if (plugin_manager_) {
            const auto& plugins = plugin_manager_->GetLoadedPlugins();
            ImGui::Separator();
            ImGui::Text("Loaded Lua Plugins (%zu):", plugins.size());
            if (plugins.empty()) {
                ImGui::TextDisabled("  No plugins loaded. Place .lua scripts in plugins/ folder.");
            } else {
                for (const auto& p : plugins) {
                    const auto& info = p->GetInfo();
                    ImGui::BulletText("%s v%s  —  %s",
                        info.name.c_str(),
                        info.version.c_str(),
                        info.description.empty() ? "" : info.description.c_str());
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::RenderSourceControl() {
    auto& git = GitManager::Instance();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);

    if (!git.HasRepo()) {
        ImGui::TextDisabled("No Git repository found in open workspace.");
        ImGui::Spacing();
        if (ImGui::Button("Initialize Repository")) {
            platform::RunCommand("git init", file_explorer_.Root());
            git.Refresh();
        }
        return;
    }

    // Branch info and refresh button
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "Branch: %s", git.GetBranch().c_str());

    float icon_sz = 14.0f * ui_scale_;
    float btn_w = icon_sz + ImGui::GetStyle().FramePadding.x * 2.0f;
    float right_x = ImGui::GetWindowContentRegionMax().x - btn_w - 4.0f;
    if (right_x > ImGui::GetCursorPosX()) {
        ImGui::SameLine(right_x);
    } else {
        ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));

    ImTextureID refresh_icon = IconManager::Instance().GetIconByName("refresh");
    if (refresh_icon) {
        if (ImGui::ImageButton("##git_refresh", refresh_icon, ImVec2(icon_sz, icon_sz))) {
            git.Refresh();
        }
    } else {
        if (ImGui::SmallButton("↻##git_refresh")) {
            git.Refresh();
        }
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh Git Status");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Commit message input
    static char commit_msg[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##commit_msg", "Message (Ctrl+Enter to commit)", commit_msg, sizeof(commit_msg));

    bool trigger_commit = false;
    if (ImGui::IsItemFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        trigger_commit = true;
    }

    ImGui::Spacing();
    if (ImGui::Button("Commit", ImVec2(-1, 0)) || trigger_commit) {
        if (strlen(commit_msg) > 0) {
            if (git.Commit(commit_msg)) {
                commit_msg[0] = '\0';
                toast_manager_.ShowSuccess("Git: Changes committed successfully.");
            } else {
                toast_manager_.ShowError("Git: Commit failed. Stage changes first.");
            }
        } else {
            toast_manager_.ShowWarning("Git: Please enter a commit message.");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Staged Changes ──────────────────────────────────────────────────
    const auto& staged = git.GetStagedChanges();
    if (!staged.empty()) {
        std::string staged_header = "STAGED CHANGES (" + std::to_string(staged.size()) + ")";
        if (ImGui::CollapsingHeader(staged_header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            float action_w = 20.0f;
            float right_pos = ImGui::GetWindowContentRegionMax().x - action_w - 4.0f;
            if (right_pos > ImGui::GetCursorPosX()) ImGui::SameLine(right_pos);
            if (ImGui::SmallButton("-##unstage_all")) {
                git.UnstageAll();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unstage All Changes");

            for (const auto& item : staged) {
                ImGui::PushID(item.path.c_str());
                
                ImVec4 col = (item.type == GitStatusType::Added) ? ImVec4(0.45f, 0.79f, 0.57f, 1.0f) :
                             (item.type == GitStatusType::Deleted) ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f) :
                             ImVec4(0.89f, 0.75f, 0.55f, 1.0f);
                char code = (item.type == GitStatusType::Added) ? 'A' :
                            (item.type == GitStatusType::Deleted) ? 'D' : 'M';

                ImGui::TextColored(col, "%c", code);
                ImGui::SameLine();
                if (ImGui::Selectable(item.path.c_str(), false, ImGuiSelectableFlags_AllowOverlap)) {
                    std::string full_path = git.GetRepoPath() + "/" + item.path;
                    tab_bar_.OpenFile(full_path, &theme_manager_.Active());
                }

                float item_btn_w = 20.0f;
                float item_right = ImGui::GetWindowContentRegionMax().x - item_btn_w - 4.0f;
                if (item_right > ImGui::GetCursorPosX()) ImGui::SameLine(item_right);
                if (ImGui::SmallButton("-")) {
                    git.UnstageFile(item.path);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unstage");

                ImGui::PopID();
            }
        }
    }

    // ── Changes (Working Tree) ──────────────────────────────────────────
    const auto& changes = git.GetUnstagedChanges();
    std::string changes_header = "CHANGES (" + std::to_string(changes.size()) + ")";
    if (ImGui::CollapsingHeader(changes_header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!changes.empty()) {
            float action_w = 20.0f;
            float right_pos = ImGui::GetWindowContentRegionMax().x - action_w - 4.0f;
            if (right_pos > ImGui::GetCursorPosX()) ImGui::SameLine(right_pos);
            if (ImGui::SmallButton("+##stage_all")) {
                git.StageAll();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stage All Changes");
        }

        if (changes.empty()) {
            ImGui::TextDisabled("No changes detected in working tree.");
        } else {
            for (const auto& item : changes) {
                ImGui::PushID(item.path.c_str());

                ImVec4 col = (item.type == GitStatusType::Untracked) ? ImVec4(0.45f, 0.79f, 0.57f, 1.0f) :
                             (item.type == GitStatusType::Deleted) ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f) :
                             ImVec4(0.89f, 0.75f, 0.55f, 1.0f);
                char code = (item.type == GitStatusType::Untracked) ? 'U' :
                            (item.type == GitStatusType::Deleted) ? 'D' : 'M';

                ImGui::TextColored(col, "%c", code);
                ImGui::SameLine();
                if (ImGui::Selectable(item.path.c_str(), false, ImGuiSelectableFlags_AllowOverlap)) {
                    std::string full_path = git.GetRepoPath() + "/" + item.path;
                    tab_bar_.OpenFile(full_path, &theme_manager_.Active());
                }

                float item_btns_w = 46.0f;
                float item_right = ImGui::GetWindowContentRegionMax().x - item_btns_w - 4.0f;
                if (item_right > ImGui::GetCursorPosX()) ImGui::SameLine(item_right);
                if (ImGui::SmallButton("+")) {
                    git.StageFile(item.path);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stage");

                ImGui::SameLine();
                if (ImGui::SmallButton("↺")) {
                    git.DiscardChanges(item.path);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Discard Changes");

                ImGui::PopID();
            }
        }
    }
}

// ── Status bar ────────────────────────────────────────────────────────────

/// Render a status bar at the bottom of the viewport showing the language,
/// cursor position, encoding, and indent style.
void App::RenderStatusBar() {
    const Theme& t = theme_manager_.Active();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float bar_height  = ImGui::GetFrameHeight() + 4.0f;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - bar_height));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_height));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, t.statusbar_bg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));

    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing);

    ImGui::PushStyleColor(ImGuiCol_Text, t.statusbar_fg);

    // Git branch (if in a repo)
    if (GitManager::Instance().HasRepo()) {
        std::string branch_str = "git: " + GitManager::Instance().GetBranch();
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "%s", branch_str.c_str());
        ImGui::SameLine(130);
    }

    // Language.
    if (auto* tab = tab_bar_.ActiveTab()) {
        ImGui::Text("%s", tab->highlighter->GetLanguageName());
        ImGui::SameLine(200);

        // Cursor position.
        auto& pos = tab->editor.GetCursors().Primary().position;
        ImGui::Text("Ln %d, Col %d", pos.line + 1, pos.column + 1);
        ImGui::SameLine(380);

        // Encoding.
        ImGui::Text("UTF-8");
        ImGui::SameLine(460);

        // Indent.
        if (tab->editor.use_spaces)
            ImGui::Text("Spaces: %d", tab->editor.tab_size);
        else
            ImGui::Text("Tab Size: %d", tab->editor.tab_size);
    } else {
        ImGui::Text("No file open");
    }

    // Right-aligned: theme name + version.
    std::string right_text = "Luce v" LUCE_VERSION;
    float right_width = ImGui::CalcTextSize(right_text.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - right_width - 12);
    ImGui::Text("%s", right_text.c_str());

    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ── Commands ──────────────────────────────────────────────────────────────

void App::RegisterCommands() {
    command_palette_.RegisterCommand({"file.new", "New File", "Ctrl+N", [this]() {
        tab_bar_.NewFile(&theme_manager_.Active());
    }});
    command_palette_.RegisterCommand({"file.open", "Open File...", "Ctrl+O", [this]() {
        std::string p = platform::OpenFileDialog();
        if (!p.empty()) tab_bar_.OpenFile(p, &theme_manager_.Active());
    }});
    command_palette_.RegisterCommand({"file.save", "Save", "Ctrl+S", [this]() {
        tab_bar_.SaveActive();
    }});
    command_palette_.RegisterCommand({"file.open_folder", "Open Folder...", "", [this]() {
        std::string folder = platform::OpenFolderDialog();
        if (!folder.empty()) {
            file_explorer_.SetRoot(folder);
            terminal_.SetWorkingDirectory(folder);
            ScanProjectFiles();
        }
    }});
    command_palette_.RegisterCommand({"view.toggle_terminal", "Toggle Terminal", "Ctrl+`", [this]() {
        show_terminal_ = !show_terminal_;
    }});
    command_palette_.RegisterCommand({"view.toggle_explorer", "Toggle Explorer", "", [this]() {
        show_file_explorer_ = !show_file_explorer_;
        if (show_file_explorer_) show_source_control_ = false;
    }});
    command_palette_.RegisterCommand({"view.toggle_source_control", "View: Toggle Source Control", "", [this]() {
        show_source_control_ = !show_source_control_;
        if (show_source_control_) {
            show_file_explorer_ = false;
            show_plugins_ = false;
            GitManager::Instance().Refresh();
        }
    }});
    command_palette_.RegisterCommand({"git.refresh", "Git: Refresh Status", "", [this]() {
        GitManager::Instance().Refresh();
        toast_manager_.ShowInfo("Git: Status refreshed.");
    }});
    command_palette_.RegisterCommand({"git.stage_all", "Git: Stage All Changes", "", [this]() {
        if (GitManager::Instance().StageAll()) {
            toast_manager_.ShowSuccess("Git: Staged all changes.");
        }
    }});
    command_palette_.RegisterCommand({"git.unstage_all", "Git: Unstage All Changes", "", [this]() {
        if (GitManager::Instance().UnstageAll()) {
            toast_manager_.ShowInfo("Git: Unstaged all changes.");
        }
    }});
    command_palette_.RegisterCommand({"tools.check_diagnostics", "Diagnostics: Check Active File", "Ctrl+Shift+B", [this]() {
        if (auto* tab = tab_bar_.ActiveTab()) {
            if (!tab->filepath.empty()) {
                tab_bar_.SaveActive();
                DiagnosticRunner::Instance().CheckFile(tab->filepath);
                show_terminal_ = true;
                toast_manager_.ShowInfo("Diagnostics: Checked " + platform::GetFilename(tab->filepath));
            } else {
                toast_manager_.ShowWarning("Diagnostics: Please save file first.");
            }
        }
    }});
    command_palette_.RegisterCommand({"markdown.preview", "Markdown: Open Preview", "Ctrl+Shift+M", [this]() {
        tab_bar_.ToggleActiveMarkdownPreview();
    }});
    command_palette_.RegisterCommand({"view.cycle_theme", "Cycle Theme", "", [this]() {
        theme_manager_.CycleTheme();
    }});
    command_palette_.RegisterCommand({"plugins.reload", "Plugins: Reload All Plugins", "", [this]() {
        if (plugin_manager_) {
            plugin_manager_->ReloadPlugins();
            toast_manager_.ShowSuccess("Plugins: Reloaded all plugins.");
        }
    }});
    
    command_palette_.RegisterCommand({"diagnostic.add_dummy", "Debug: Add Dummy Error", "", [this]() {
        Diagnostic diag;
        diag.file_path = "src/main.cpp"; // Just a test file
        diag.line = 10;
        diag.column = 1;
        diag.message = "Dummy Error: Undefined identifier 'test'";
        diag.severity = DiagnosticSeverity::Error;
        DiagnosticManager::Instance().AddDiagnostic(diag);
        
        diag.file_path = "src/ui/app.cpp";
        diag.line = 100;
        diag.column = 15;
        diag.message = "Dummy Warning: Unused variable";
        diag.severity = DiagnosticSeverity::Warning;
        DiagnosticManager::Instance().AddDiagnostic(diag);
        
        show_terminal_ = true; // Open bottom panel to show it
    }});
    
    command_palette_.RegisterCommand({"diagnostic.clear", "Debug: Clear Diagnostics", "", []() {
        DiagnosticManager::Instance().ClearDiagnostics();
    }});

    command_palette_.RegisterCommand({"debug.toast_info", "Debug: Show Info Notification", "", [this]() {
        toast_manager_.ShowInfo("Luce: Build succeeded with 0 errors.");
    }});
    command_palette_.RegisterCommand({"debug.toast_warn", "Debug: Show Warning Notification", "", [this]() {
        toast_manager_.ShowWarning("Warning: Deprecated compiler flag detected in build configuration.");
    }});
    command_palette_.RegisterCommand({"debug.toast_error", "Debug: Show Error Notification", "", [this]() {
        toast_manager_.ShowError("Failed to start CMake Client: Unsupported toolchain.");
    }});

    command_palette_.RegisterCommand({"view.zoom_in", "View: Zoom In", "Ctrl+=", [this]() {
        ZoomIn();
        SaveSession();
    }});
    command_palette_.RegisterCommand({"view.zoom_out", "View: Zoom Out", "Ctrl+-", [this]() {
        ZoomOut();
        SaveSession();
    }});
    command_palette_.RegisterCommand({"view.zoom_reset", "View: Reset Zoom", "Ctrl+0", [this]() {
        ResetZoom();
        SaveSession();
    }});

    for (auto& name : theme_manager_.GetThemeNames()) {
        command_palette_.RegisterCommand({"theme." + name, "Theme: " + name, "", [this, name]() {
            theme_manager_.SetTheme(name);
        }});
    }

    command_palette_.RegisterCommand({"theme.reload", "Theme: Reload Custom Themes", "", [this]() {
        theme_manager_.ReloadThemes();
        // Note: For newly added themes to appear in the palette without restarting,
        // we'd need to clear and re-register commands. For now, this just reloads the CSS 
        // files and applies any changes to the CURRENT custom theme instantly.
    }});
}

// ── Project file scanning ─────────────────────────────────────────────────

/// Recursively scan the project root for files (used by Quick Open).
void App::ScanProjectFiles() {
    std::vector<std::string> files;
    std::string root = file_explorer_.Root();
    if (root.empty()) return;

    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        std::string name = entry.path().filename().string();
        // Skip hidden files and common noisy directories.
        std::string path_str = entry.path().string();
        if (path_str.find(".git") != std::string::npos) continue;
        if (path_str.find("node_modules") != std::string::npos) continue;
        if (path_str.find("build") != std::string::npos) continue;
        if (path_str.find("target") != std::string::npos) continue;

        std::ranges::replace(path_str, '\\', '/');
        // Store relative path for cleaner display.
        if (path_str.starts_with(root)) {
            path_str = path_str.substr(root.size());
            if (!path_str.empty() && path_str[0] == '/') path_str = path_str.substr(1);
        }
        files.push_back(path_str);

        // Limit to avoid scanning enormous trees.
        if (files.size() > 10000) break;
    }

    std::ranges::sort(files);
    command_palette_.SetProjectFiles(files);
}

void App::LoadSession() {
    std::string config_path = platform::GetExecutableDir() + "/session.json";
    if (!fs::exists(config_path)) return;

    std::ifstream file(config_path);
    if (!file.is_open()) return;

    try {
        json j;
        file >> j;

        if (j.contains("folder") && j["folder"].is_string()) {
            std::string saved_folder = j["folder"].get<std::string>();
            if (!saved_folder.empty() && fs::is_directory(saved_folder)) {
                file_explorer_.SetRoot(saved_folder);
                terminal_.SetWorkingDirectory(saved_folder);
                ScanProjectFiles();
            }
        }

        if (j.contains("scale") && j["scale"].is_number()) {
            SetScale(j["scale"].get<float>());
        }

        if (j.contains("files") && j["files"].is_array()) {
            for (const auto& item : j["files"]) {
                if (item.is_string()) {
                    std::string f = item.get<std::string>();
                    if (fs::exists(f)) {
                        tab_bar_.OpenFile(f, &theme_manager_.Active());
                    }
                }
            }
        }
    } catch (...) {
        // Ignore corrupted session file
    }
}

void App::SaveSession() {
    std::string config_path = platform::GetExecutableDir() + "/session.json";
    std::ofstream file(config_path, std::ios::trunc);
    if (!file.is_open()) return;

    json j;
    j["folder"] = file_explorer_.Root();
    j["scale"] = ui_scale_;

    std::vector<std::string> files;
    for (const auto& tab : tab_bar_.GetTabs()) {
        if (tab && !tab->filepath.empty()) {
            files.push_back(tab->filepath);
        }
    }
    j["files"] = files;

    file << j.dump(4);
}

void App::OnFileDrop(const std::string& path) {
    if (fs::is_directory(path)) {
        file_explorer_.SetRoot(path);
        terminal_.SetWorkingDirectory(path);
        ScanProjectFiles();
        SaveSession();
    } else {
        tab_bar_.OpenFile(path, &theme_manager_.Active());
        SaveSession();
    }
}

}  // namespace luce
