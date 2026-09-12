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
    file_explorer_.SetOnCloneRepository([this]() {
        show_git_clone_modal_ = true;
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
    command_palette_.SetOnOpenFileAtLine([this](const std::string& path, int line, int col) {
        tab_bar_.OpenFile(path, &theme_manager_.Active());
        if (auto* editor = tab_bar_.ActiveEditor()) {
            int target_line = (line > 0) ? (line - 1) : 0;
            int target_col  = (col > 0)  ? (col - 1)  : 0;
            editor->GoToPosition(target_line, target_col);
            editor->EnsureCursorVisible();
        }
        SaveSession();
    });
    command_palette_.SetOnGoToLine([this](int line) {
        if (auto* editor = tab_bar_.ActiveEditor()) {
            editor->GoToPosition(line, 0);
            editor->EnsureCursorVisible();
        }
    });

    // Wire up SymbolIndex, Minimap and Go to Definition on TabBar
    tab_bar_.SetSymbolIndex(&symbol_index_);
    tab_bar_.SetMinimapEnabled(show_minimap_);
    tab_bar_.SetOnGoToDefinition([this](const std::string& path, int line) {
        tab_bar_.OpenFile(path, &theme_manager_.Active());
        if (auto* editor = tab_bar_.ActiveEditor()) {
            editor->GoToLine(line > 0 ? line - 1 : 0);
            editor->EnsureCursorVisible();
        }
        SaveSession();
    });

    // Wire up plugin event hooks on TabBar
    tab_bar_.SetOnFileOpened([this](const std::string& path) {
        if (plugin_manager_) plugin_manager_->OnFileOpened(path);
    });
    tab_bar_.SetOnBeforeSave([this](const std::string& path) {
        if (plugin_manager_) plugin_manager_->OnBeforeSave(path);
    });
    tab_bar_.SetOnAfterSave([this](const std::string& path) {
        if (plugin_manager_) plugin_manager_->OnAfterSave(path);
    });
    tab_bar_.SetOnTextChanged([this](int line, int count) {
        if (plugin_manager_) plugin_manager_->OnTextChanged(line, count);
    });
    tab_bar_.SetCompletionProvider([this](const std::string& ext, const std::string& prefix, int line, int col) {
        if (plugin_manager_) {
            return plugin_manager_->GetCompletions(ext, prefix, line, col);
        }
        return std::vector<std::string>{};
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

App::~App() {
    SaveSession();
}

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

    // Git Modals (Confirmation, Branches, Remotes, Stashes, Tags, Clone, Output)
    RenderGitModals();

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

        ImVec2 active_btn_min(0, 0);
        ImVec2 active_btn_max(0, 0);

        // Explorer Button
        ImTextureID explorer_icon = IconManager::Instance().GetIconByName("default_folder");
        if (explorer_icon) {
            if (ImGui::ImageButton("##horiz_explorer", explorer_icon, ImVec2(icon_size, icon_size))) {
                show_file_explorer_ = true;
                show_source_control_ = false;
                show_plugins_ = false;
            }
            if (show_file_explorer_) {
                active_btn_min = ImGui::GetItemRectMin();
                active_btn_max = ImGui::GetItemRectMax();
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
                GitManager::Instance().RefreshAsync();
            }
            if (show_source_control_) {
                active_btn_min = ImGui::GetItemRectMin();
                active_btn_max = ImGui::GetItemRectMax();
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
            if (show_plugins_) {
                active_btn_min = ImGui::GetItemRectMin();
                active_btn_max = ImGui::GetItemRectMax();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Plugins");
        }
        ImGui::PopStyleColor(3);

        // Active indicator line positioned precisely under the active button
        if (active_btn_max.x > active_btn_min.x) {
            float line_y = active_btn_max.y + 2.0f;
            float pad_x = 4.0f * ui_scale_;
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddLine(
                ImVec2(active_btn_min.x + pad_x, line_y), 
                ImVec2(active_btn_max.x - pad_x, line_y), 
                IM_COL32(0, 122, 204, 255), 2.0f * ui_scale_);
        }

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
                    ImGui::Text("%zu problem%s", diags.size(), diags.size() == 1 ? "" : "s");
                    ImGui::SameLine(0, 15.0f);
                    if (ImGui::SmallButton("Copy All")) {
                        std::string all_text;
                        for (const auto& d : diags) {
                            std::string sev = (d.severity == DiagnosticSeverity::Error) ? "Error" :
                                              (d.severity == DiagnosticSeverity::Warning) ? "Warning" : "Info";
                            all_text += "[" + sev + "] " + d.file_path + ":" + std::to_string(d.line) + ":" + std::to_string(d.column) + " - " + d.message + "\n";
                        }
                        ImGui::SetClipboardText(all_text.c_str());
                        toast_manager_.ShowInfo("Copied all problems to clipboard");
                    }
                    ImGui::SameLine(0, 12.0f);
                    ImGui::TextDisabled("(Right-click a problem or press Ctrl+C to copy)");

                    if (ImGui::BeginTable("##diagnostics", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                        ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 60.0f);
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
                                    editor->GoToLine(diag.line > 0 ? diag.line - 1 : 0);
                                    editor->EnsureCursorVisible();
                                }
                            }

                            // Copy message with Ctrl+C when hovering over row
                            if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
                                ImGui::SetClipboardText(diag.message.c_str());
                                toast_manager_.ShowInfo("Copied error message to clipboard");
                            }

                            // Right-click context menu to copy
                            if (ImGui::BeginPopupContextItem(label)) {
                                if (ImGui::MenuItem("Copy Message", "Ctrl+C")) {
                                    ImGui::SetClipboardText(diag.message.c_str());
                                    toast_manager_.ShowInfo("Copied message to clipboard");
                                }
                                if (ImGui::MenuItem("Copy Problem (with Location)")) {
                                    std::string full = diag.file_path + ":" + std::to_string(diag.line) + ":" + std::to_string(diag.column) + " - " + diag.message;
                                    ImGui::SetClipboardText(full.c_str());
                                    toast_manager_.ShowInfo("Copied problem to clipboard");
                                }
                                ImGui::Separator();
                                if (ImGui::MenuItem("Copy All Problems")) {
                                    std::string all_text;
                                    for (const auto& d : diags) {
                                        std::string sev = (d.severity == DiagnosticSeverity::Error) ? "Error" :
                                                          (d.severity == DiagnosticSeverity::Warning) ? "Warning" : "Info";
                                        all_text += "[" + sev + "] " + d.file_path + ":" + std::to_string(d.line) + ":" + std::to_string(d.column) + " - " + d.message + "\n";
                                    }
                                    ImGui::SetClipboardText(all_text.c_str());
                                    toast_manager_.ShowInfo("Copied all problems to clipboard");
                                }
                                ImGui::EndPopup();
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

    // Per-frame plugin tick and cursor movement tracking
    if (plugin_manager_) {
        plugin_manager_->Tick(ImGui::GetIO().DeltaTime);
        if (auto* ed = tab_bar_.ActiveEditor()) {
            if (!ed->GetCursors().cursors.empty()) {
                const auto& cur = ed->GetCursors().Primary();
                static int last_cursor_line = -1;
                static int last_cursor_col  = -1;
                if (cur.position.line != last_cursor_line || cur.position.column != last_cursor_col) {
                    last_cursor_line = cur.position.line;
                    last_cursor_col  = cur.position.column;
                    plugin_manager_->OnCursorMoved(cur.position.line, cur.position.column);
                }
            }
        }
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

    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F))
        command_palette_.Open(PaletteMode::ProjectSearch);

    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_M))
        tab_bar_.ToggleActiveMarkdownPreview();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Backslash))
        tab_bar_.ToggleSplitView();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G))
        command_palette_.Open(PaletteMode::GoToLine);

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))
        tab_bar_.NewFile(&theme_manager_.Active());

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        std::string path = platform::OpenFileDialog();
        if (!path.empty()) tab_bar_.OpenFile(path, &theme_manager_.Active());
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        tab_bar_.SaveActive();
        SaveSession();
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W)) {
        tab_bar_.CloseTab(tab_bar_.ActiveIndex());
        SaveSession();
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        tab_bar_.NextTab();
        SaveSession();
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_M)) {
        ToggleMinimap();
    }

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

    // Go to Definition shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_F12)) {
        if (auto* editor = tab_bar_.ActiveEditor()) {
            editor->GoToDefinition();
        }
    }
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
        if (auto* editor = tab_bar_.ActiveEditor()) {
            if (editor->HasHoveredSymbol()) {
                editor->GoToDefinition();
            }
        }
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
                symbol_index_.Clear();
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
            if (ImGui::MenuItem("Minimap", "Ctrl+M", show_minimap_)) {
                ToggleMinimap();
            }
            if (ImGui::MenuItem("Toggle Split Editor", "Ctrl+\\", tab_bar_.IsSplitView())) {
                tab_bar_.ToggleSplitView();
            }
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
                        SaveSession();
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
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
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
                for (size_t i = 0; i < plugins.size(); ++i) {
                    const auto& p = plugins[i];
                    bool enabled = p->IsEnabled();
                    std::string chk_label = "##plugin_en_" + std::to_string(i);
                    if (ImGui::Checkbox(chk_label.c_str(), &enabled)) {
                        plugin_manager_->SetPluginEnabled(i, enabled);
                    }
                    ImGui::SameLine();
                    const auto& info = p->GetInfo();
                    ImVec4 name_col = enabled ? ImVec4(0.9f, 0.9f, 0.9f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    ImGui::TextColored(name_col, "%s v%s  —  %s",
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

void App::RenderGitModals() {
    auto& git = GitManager::Instance();
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    // ── 1. Reusable Confirmation Modal (About Luce style) ───────────────
    if (confirm_modal_.request_open) {
        ImGui::OpenPopup("Confirm Action##modal");
        confirm_modal_.request_open = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f * ui_scale_, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Confirm Action##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(confirm_modal_.confirm_color, "%s", confirm_modal_.title.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", confirm_modal_.message.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float avail_w = ImGui::GetContentRegionAvail().x;
        float btn_w = (avail_w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button, confirm_modal_.confirm_color);
        if (ImGui::Button(confirm_modal_.confirm_label.c_str(), ImVec2(btn_w, 0))) {
            if (confirm_modal_.on_confirm) {
                confirm_modal_.on_confirm();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── 2. Git Branches Modal (Tabs: Switch & Create, Rename, Delete, Merge) ──
    if (show_git_branch_modal_) {
        ImGui::OpenPopup("Git Branches##modal");
        show_git_branch_modal_ = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Git Branches##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "Current Branch: %s", git.GetBranch().c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTabBar("##branch_tabs")) {
            // Tab 1: Switch & Create
            if (ImGui::BeginTabItem("Switch & Create")) {
                static char new_branch_name[64] = "";
                ImGui::Spacing();
                ImGui::Text("Create New Branch:");
                ImGui::SetNextItemWidth(260.0f);
                ImGui::InputTextWithHint("##new_branch_input", "New branch name...", new_branch_name, sizeof(new_branch_name));
                ImGui::SameLine();
                if (ImGui::Button("Create & Switch")) {
                    if (strlen(new_branch_name) > 0) {
                        std::string err;
                        if (git.CreateBranch(new_branch_name, true, err)) {
                            tab_bar_.ReloadAllFromDisk();
                            ScanProjectFiles();
                            toast_manager_.ShowSuccess("Git: Created and switched to branch '" + std::string(new_branch_name) + "'");
                            new_branch_name[0] = '\0';
                            ImGui::CloseCurrentPopup();
                        } else {
                            toast_manager_.ShowError(err);
                        }
                    } else {
                        toast_manager_.ShowWarning("Git: Branch name cannot be empty.");
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                static char branch_filter[64] = "";
                ImGui::Text("Switch Branch:");
                ImGui::SetNextItemWidth(380.0f);
                ImGui::InputTextWithHint("##filter_branch", "Filter branches...", branch_filter, sizeof(branch_filter));

                std::vector<std::string> branches = git.GetBranchList();
                std::string filter_str = branch_filter;
                std::ranges::transform(filter_str, filter_str.begin(), ::tolower);

                ImGui::BeginChild("##branch_list_child", ImVec2(380.0f, 160.0f), true);
                if (branches.empty()) {
                    ImGui::TextDisabled("No branches found.");
                } else {
                    for (const auto& b : branches) {
                        std::string b_lower = b;
                        std::ranges::transform(b_lower, b_lower.begin(), ::tolower);
                        if (!filter_str.empty() && b_lower.find(filter_str) == std::string::npos) {
                            continue;
                        }

                        bool is_current = (b == git.GetBranch());
                        std::string label = (is_current ? "* " : "  ") + b;
                        if (ImGui::Selectable(label.c_str(), is_current)) {
                            if (!is_current) {
                                std::string err;
                                if (git.CheckoutBranch(b, err)) {
                                    tab_bar_.ReloadAllFromDisk();
                                    ScanProjectFiles();
                                    toast_manager_.ShowSuccess("Git: Switched to branch '" + b + "'");
                                    ImGui::CloseCurrentPopup();
                                } else {
                                    toast_manager_.ShowError(err);
                                }
                            }
                        }
                        if (is_current && ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Current active branch");
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // Tab 2: Rename
            if (ImGui::BeginTabItem("Rename")) {
                ImGui::Spacing();
                static int rename_branch_idx = 0;
                auto branches = git.GetBranchList();
                if (branches.empty()) {
                    ImGui::TextDisabled("No branches available to rename.");
                } else {
                    if (rename_branch_idx >= (int)branches.size()) rename_branch_idx = 0;
                    ImGui::Text("Select Branch to Rename:");
                    ImGui::SetNextItemWidth(380.0f);
                    if (ImGui::BeginCombo("##rename_combo", branches[rename_branch_idx].c_str())) {
                        for (int i = 0; i < (int)branches.size(); ++i) {
                            bool is_sel = (i == rename_branch_idx);
                            if (ImGui::Selectable(branches[i].c_str(), is_sel)) rename_branch_idx = i;
                            if (is_sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    static char rename_to[64] = "";
                    ImGui::Text("New Branch Name:");
                    ImGui::SetNextItemWidth(380.0f);
                    ImGui::InputTextWithHint("##rename_to_input", "New name...", rename_to, sizeof(rename_to));

                    ImGui::Spacing();
                    if (ImGui::Button("Rename Branch", ImVec2(160.0f, 0))) {
                        if (strlen(rename_to) > 0 && rename_branch_idx < (int)branches.size()) {
                            std::string old_name = branches[rename_branch_idx];
                            std::string new_name = rename_to;
                            std::string err;
                            if (git.RenameBranch(old_name, new_name, err)) {
                                toast_manager_.ShowSuccess("Git: Renamed branch '" + old_name + "' to '" + new_name + "'");
                                rename_to[0] = '\0';
                                ImGui::CloseCurrentPopup();
                            } else {
                                toast_manager_.ShowError(err);
                            }
                        } else {
                            toast_manager_.ShowWarning("Please provide a new branch name.");
                        }
                    }
                }
                ImGui::EndTabItem();
            }

            // Tab 3: Delete
            if (ImGui::BeginTabItem("Delete")) {
                ImGui::Spacing();
                static int del_branch_idx = 0;
                static bool force_del = false;
                auto branches = git.GetBranchList();
                if (branches.empty()) {
                    ImGui::TextDisabled("No branches available to delete.");
                } else {
                    if (del_branch_idx >= (int)branches.size()) del_branch_idx = 0;
                    ImGui::Text("Select Branch to Delete:");
                    ImGui::SetNextItemWidth(380.0f);
                    if (ImGui::BeginCombo("##del_combo", branches[del_branch_idx].c_str())) {
                        for (int i = 0; i < (int)branches.size(); ++i) {
                            bool is_sel = (i == del_branch_idx);
                            std::string item_label = branches[i];
                            if (branches[i] == git.GetBranch()) item_label += " (current)";
                            if (ImGui::Selectable(item_label.c_str(), is_sel)) del_branch_idx = i;
                            if (is_sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Checkbox("Force Delete (-D)", &force_del);

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
                    if (ImGui::Button("Delete Branch", ImVec2(160.0f, 0))) {
                        if (del_branch_idx < (int)branches.size()) {
                            std::string target_branch = branches[del_branch_idx];
                            if (target_branch == git.GetBranch()) {
                                toast_manager_.ShowError("Cannot delete active branch. Switch branch first.");
                            } else {
                                bool do_force = force_del;
                                RequestConfirmation("Delete Branch",
                                    "Are you sure you want to delete branch '" + target_branch + "'?" +
                                    (do_force ? "\nWarning: Force delete (-D) may discard unmerged commits!" : ""),
                                    "Delete", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
                                    [this, target_branch, do_force]() {
                                        std::string err;
                                        if (GitManager::Instance().DeleteBranch(target_branch, do_force, err)) {
                                            toast_manager_.ShowSuccess("Git: Deleted branch '" + target_branch + "'");
                                        } else {
                                            toast_manager_.ShowError(err);
                                        }
                                    });
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                    ImGui::PopStyleColor();
                }
                ImGui::EndTabItem();
            }

            // Tab 4: Merge
            if (ImGui::BeginTabItem("Merge")) {
                ImGui::Spacing();
                static int merge_branch_idx = 0;
                auto branches = git.GetBranchList();
                std::vector<std::string> other_branches;
                for (const auto& b : branches) {
                    if (b != git.GetBranch()) other_branches.push_back(b);
                }

                if (other_branches.empty()) {
                    ImGui::TextDisabled("No other branches to merge into '%s'.", git.GetBranch().c_str());
                } else {
                    if (merge_branch_idx >= (int)other_branches.size()) merge_branch_idx = 0;
                    ImGui::Text("Select branch to merge INTO current branch '%s':", git.GetBranch().c_str());
                    ImGui::SetNextItemWidth(380.0f);
                    if (ImGui::BeginCombo("##merge_combo", other_branches[merge_branch_idx].c_str())) {
                        for (int i = 0; i < (int)other_branches.size(); ++i) {
                            bool is_sel = (i == merge_branch_idx);
                            if (ImGui::Selectable(other_branches[i].c_str(), is_sel)) merge_branch_idx = i;
                            if (is_sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("Merge into Current Branch", ImVec2(220.0f, 0))) {
                        std::string target_branch = other_branches[merge_branch_idx];
                        std::string cur_branch = git.GetBranch();
                        RequestConfirmation("Merge Branch",
                            "Are you sure you want to merge branch '" + target_branch + "' into '" + cur_branch + "'?",
                            "Merge", ImVec4(0.2f, 0.55f, 0.85f, 1.0f),
                            [this, target_branch]() {
                                std::string err;
                                if (GitManager::Instance().MergeBranch(target_branch, err)) {
                                    tab_bar_.ReloadAllFromDisk();
                                    ScanProjectFiles();
                                    toast_manager_.ShowSuccess("Git: Merged '" + target_branch + "' successfully.");
                                } else {
                                    toast_manager_.ShowError(err);
                                }
                            });
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── 3. Git Remotes Modal ────────────────────────────────────────────
    if (show_git_remote_modal_) {
        ImGui::OpenPopup("Git Remotes##modal");
        show_git_remote_modal_ = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Git Remotes##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char remote_name[64] = "origin";
        static char remote_url[256] = "";

        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "Manage Remote Repositories");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Add Remote Section
        ImGui::Text("Add Remote:");
        ImGui::Text("Name:");
        ImGui::SetNextItemWidth(380.0f);
        ImGui::InputText("##remote_name_input", remote_name, sizeof(remote_name));

        ImGui::Text("URL:");
        ImGui::SetNextItemWidth(380.0f);
        ImGui::InputTextWithHint("##remote_url_input", "https://github.com/user/repo.git", remote_url, sizeof(remote_url));

        ImGui::Spacing();
        if (ImGui::Button("Add Remote", ImVec2(140.0f, 0))) {
            if (strlen(remote_url) > 0) {
                std::string err;
                if (git.AddRemote(remote_name, remote_url, err)) {
                    toast_manager_.ShowSuccess("Git: Added remote '" + std::string(remote_name) + "'");
                    remote_url[0] = '\0';
                } else {
                    toast_manager_.ShowError(err);
                }
            } else {
                toast_manager_.ShowWarning("Remote URL cannot be empty.");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Configured Remotes
        ImGui::Text("Configured Remotes:");
        auto remotes = git.GetRemoteList();
        if (remotes.empty()) {
            ImGui::TextDisabled("No remotes configured.");
        } else {
            ImGui::BeginChild("##remotes_list_child", ImVec2(380.0f, 120.0f), true);
            for (const auto& r : remotes) {
                ImGui::PushID(r.c_str());
                std::string url = git.GetRemoteUrl(r);
                ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "%s", r.c_str());
                if (!url.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", url.c_str());
                }
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 70.0f);
                if (ImGui::SmallButton("Remove")) {
                    RequestConfirmation("Remove Remote",
                        "Are you sure you want to remove remote '" + r + "'?",
                        "Remove", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
                        [this, r]() {
                            std::string err;
                            if (GitManager::Instance().RemoveRemote(r, err)) {
                                toast_manager_.ShowSuccess("Git: Removed remote '" + r + "'");
                            } else {
                                toast_manager_.ShowError(err);
                            }
                        });
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── 4. Git Stash Modal ──────────────────────────────────────────────
    if (show_git_stash_modal_) {
        ImGui::OpenPopup("Git Stash##modal");
        show_git_stash_modal_ = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Git Stash##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "Stash Management");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        static char stash_msg[128] = "";
        static bool stash_untracked = true;
        static bool stash_keep_index = false;

        ImGui::Text("Save Stash:");
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputTextWithHint("##stash_msg_input", "Optional stash description...", stash_msg, sizeof(stash_msg));
        ImGui::Checkbox("Include untracked (-u)", &stash_untracked);
        ImGui::SameLine(220.0f);
        ImGui::Checkbox("Keep index (keep staged)", &stash_keep_index);

        ImGui::Spacing();
        if (ImGui::Button("Stash Changes", ImVec2(140.0f, 0))) {
            std::string err;
            if (git.StashSave(stash_msg, stash_untracked, stash_keep_index, err)) {
                tab_bar_.ReloadAllFromDisk();
                toast_manager_.ShowSuccess("Git: Stashed changes successfully.");
                stash_msg[0] = '\0';
            } else {
                toast_manager_.ShowError(err);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Existing Stashes:");
        auto stashes = git.GetStashList();
        if (stashes.empty()) {
            ImGui::TextDisabled("No stashes available.");
        } else {
            ImGui::BeginChild("##stash_list_child", ImVec2(420.0f, 160.0f), true);
            for (const auto& s : stashes) {
                ImGui::PushID(s.name.c_str());
                ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "%s", s.name.c_str());
                ImGui::SameLine();
                ImGui::TextWrapped("%s", s.message.c_str());

                if (ImGui::SmallButton("Apply")) {
                    std::string err;
                    if (git.StashApply(s.index, err)) {
                        tab_bar_.ReloadAllFromDisk();
                        toast_manager_.ShowSuccess("Git: Applied " + s.name);
                    } else {
                        toast_manager_.ShowError(err);
                    }
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Pop")) {
                    std::string err;
                    if (git.StashPop(s.index, err)) {
                        tab_bar_.ReloadAllFromDisk();
                        toast_manager_.ShowSuccess("Git: Popped " + s.name);
                    } else {
                        toast_manager_.ShowError(err);
                    }
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Drop")) {
                    int s_idx = s.index;
                    std::string s_name = s.name;
                    RequestConfirmation("Drop Stash",
                        "Are you sure you want to drop " + s_name + "?\nThis cannot be recovered!",
                        "Drop", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
                        [this, s_idx, s_name]() {
                            std::string err;
                            if (GitManager::Instance().StashDrop(s_idx, err)) {
                                toast_manager_.ShowSuccess("Git: Dropped " + s_name);
                            } else {
                                toast_manager_.ShowError(err);
                            }
                        });
                }

                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── 5. Git Tags Modal ───────────────────────────────────────────────
    if (show_git_tags_modal_) {
        ImGui::OpenPopup("Git Tags##modal");
        show_git_tags_modal_ = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Git Tags##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "Git Tags");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        static char tag_name[64] = "";
        static char tag_msg[128] = "";

        ImGui::Text("Create Tag:");
        ImGui::SetNextItemWidth(380.0f);
        ImGui::InputTextWithHint("##tag_name_input", "Tag name (e.g. v1.0.0)...", tag_name, sizeof(tag_name));

        ImGui::Text("Message (optional):");
        ImGui::SetNextItemWidth(380.0f);
        ImGui::InputTextWithHint("##tag_msg_input", "Tag annotation message...", tag_msg, sizeof(tag_msg));

        ImGui::Spacing();
        if (ImGui::Button("Create Tag", ImVec2(140.0f, 0))) {
            if (strlen(tag_name) > 0) {
                std::string err;
                if (git.CreateTag(tag_name, tag_msg, err)) {
                    toast_manager_.ShowSuccess("Git: Created tag '" + std::string(tag_name) + "'");
                    tag_name[0] = '\0';
                    tag_msg[0] = '\0';
                } else {
                    toast_manager_.ShowError(err);
                }
            } else {
                toast_manager_.ShowWarning("Tag name cannot be empty.");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Existing Tags:");
        auto tags = git.GetTagList();
        if (tags.empty()) {
            ImGui::TextDisabled("No tags found.");
        } else {
            ImGui::BeginChild("##tags_list_child", ImVec2(380.0f, 140.0f), true);
            for (const auto& tg : tags) {
                ImGui::PushID(tg.c_str());
                ImGui::Text("%s", tg.c_str());
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 65.0f);
                if (ImGui::SmallButton("Delete")) {
                    RequestConfirmation("Delete Tag",
                        "Are you sure you want to delete tag '" + tg + "'?",
                        "Delete", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
                        [this, tg]() {
                            std::string err;
                            if (GitManager::Instance().DeleteTag(tg, err)) {
                                toast_manager_.ShowSuccess("Git: Deleted tag '" + tg + "'");
                            } else {
                                toast_manager_.ShowError(err);
                            }
                        });
                }
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::Spacing();
            if (ImGui::Button("Push All Tags to Remote", ImVec2(200.0f, 0))) {
                std::string err;
                if (git.PushTags(err)) {
                    toast_manager_.ShowSuccess("Git: Pushed all tags.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── 6. Git Clone Modal ──────────────────────────────────────────────
    if (show_git_clone_modal_) {
        ImGui::OpenPopup("Clone Repository##modal");
        show_git_clone_modal_ = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Clone Repository##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char clone_url[256] = "";
        static char clone_target[256] = "";

        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "Clone Remote Repository");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Repository URL:");
        ImGui::SetNextItemWidth(450.0f);
        ImGui::InputTextWithHint("##clone_url", "https://github.com/user/repository.git", clone_url, sizeof(clone_url));

        ImGui::Text("Target Directory:");
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputTextWithHint("##clone_target", "C:/path/to/destination", clone_target, sizeof(clone_target));
        ImGui::SameLine();
        if (ImGui::Button("Browse...##clone_browse")) {
            std::string folder = platform::OpenFolderDialog();
            if (!folder.empty()) {
                strncpy_s(clone_target, folder.c_str(), sizeof(clone_target) - 1);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float clone_avail_w = ImGui::GetContentRegionAvail().x;
        float clone_btn_w = (clone_avail_w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Clone", ImVec2(clone_btn_w, 0))) {
            if (strlen(clone_url) > 0 && strlen(clone_target) > 0) {
                std::string err;
                toast_manager_.ShowInfo("Git: Cloning repository in progress...");
                if (GitManager::CloneRepo(clone_url, clone_target, err)) {
                    file_explorer_.SetRoot(clone_target);
                    terminal_.SetWorkingDirectory(clone_target);
                    ScanProjectFiles();
                    git.SetRepoPath(clone_target);
                    SaveSession();
                    toast_manager_.ShowSuccess("Git: Cloned and opened workspace successfully!");
                    clone_url[0] = '\0';
                    clone_target[0] = '\0';
                    ImGui::CloseCurrentPopup();
                } else {
                    toast_manager_.ShowError(err);
                }
            } else {
                toast_manager_.ShowWarning("Please provide both URL and target directory.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(clone_btn_w, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── 7. Git Output Log Modal ─────────────────────────────────────────
    if (show_git_output_modal_) {
        ImGui::OpenPopup("Git Output##modal");
        show_git_output_modal_ = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(650.0f * ui_scale_, 450.0f * ui_scale_), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Git Output##modal", nullptr)) {
        auto logs = git.GetCommandLog();

        if (ImGui::Button("Copy All")) {
            std::string full_log;
            for (const auto& l : logs) {
                full_log += "[" + l.timestamp + "] " + l.command + " (exit: " + std::to_string(l.exit_code) + ")\n";
                if (!l.output.empty()) full_log += l.output + "\n";
            }
            ImGui::SetClipboardText(full_log.c_str());
            toast_manager_.ShowInfo("Git Output copied to clipboard.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Output")) {
            git.ClearCommandLog();
        }
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 70.0f);
        if (ImGui::Button("Close", ImVec2(60.0f, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();

        ImGui::BeginChild("##git_output_scroll", ImVec2(0, 0), true);
        if (logs.empty()) {
            ImGui::TextDisabled("No Git commands recorded yet.");
        } else {
            for (const auto& l : logs) {
                ImGui::TextDisabled("[%s]", l.timestamp.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", l.command.c_str());
                ImGui::SameLine();
                if (l.exit_code == 0) {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[ok]");
                } else {
                    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[exit %d]", l.exit_code);
                }

                if (!l.output.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    ImGui::Indent(16.0f);
                    ImGui::TextWrapped("%s", l.output.c_str());
                    ImGui::Unindent(16.0f);
                    ImGui::PopStyleColor();
                }
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    RenderGitDiffModal();
}

void App::ShowGitDiffModal(const std::string& path) {
    git_diff_file_ = path;
    git_diff_content_ = GitManager::Instance().GetFileDiff(path);
    show_git_diff_modal_ = true;
}

void App::RenderGitDiffModal() {
    if (show_git_diff_modal_) {
        ImGui::OpenPopup("Git Diff##modal");
        show_git_diff_modal_ = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(750.0f * ui_scale_, 520.0f * ui_scale_), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Git Diff##modal", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1.0f), "Diff: %s", git_diff_file_.c_str());
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 200.0f);

        if (ImGui::Button("Refresh")) {
            git_diff_content_ = GitManager::Instance().GetFileDiff(git_diff_file_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stage")) {
            GitManager::Instance().StageFile(git_diff_file_);
            git_diff_content_ = GitManager::Instance().GetFileDiff(git_diff_file_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(60.0f, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("##git_diff_scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (font_editor_) ImGui::PushFont(font_editor_);

        std::istringstream iss(git_diff_content_);
        std::string line;
        while (std::getline(iss, line)) {
            ImVec4 text_col = theme_manager_.Active().foreground;
            ImU32 bg_col = 0;

            if (line.starts_with("+++") || line.starts_with("---") || line.starts_with("diff ") || line.starts_with("index ")) {
                text_col = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
            } else if (line.starts_with("@@")) {
                text_col = ImVec4(0.3f, 0.8f, 0.95f, 1.0f);
                bg_col = IM_COL32(20, 50, 70, 70);
            } else if (line.starts_with("+")) {
                text_col = ImVec4(0.35f, 0.88f, 0.45f, 1.0f);
                bg_col = IM_COL32(30, 80, 45, 75);
            } else if (line.starts_with("-")) {
                text_col = ImVec4(0.95f, 0.4f, 0.4f, 1.0f);
                bg_col = IM_COL32(95, 30, 30, 75);
            }

            if (bg_col != 0) {
                ImVec2 p_min = ImGui::GetCursorScreenPos();
                float line_h = ImGui::GetTextLineHeight();
                float full_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
                ImGui::GetWindowDrawList()->AddRectFilled(
                    p_min,
                    ImVec2(p_min.x + (std::max)(full_w, ImGui::CalcTextSize(line.c_str()).x + 20.0f), p_min.y + line_h),
                    bg_col
                );
            }

            ImGui::TextColored(text_col, "%s", line.c_str());
        }

        if (font_editor_) ImGui::PopFont();
        ImGui::EndChild();
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
            git.RefreshAsync();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clone Repository...")) {
            show_git_clone_modal_ = true;
        }
        return;
    }

    // Branch selector combo and action buttons
    float icon_sz = 14.0f * ui_scale_;
    float refresh_btn_w = icon_sz + ImGui::GetStyle().FramePadding.x * 2.0f;
    float more_btn_w = 22.0f * ui_scale_;
    float total_right_w = refresh_btn_w + more_btn_w + ImGui::GetStyle().ItemSpacing.x;

    float branch_text_w = ImGui::CalcTextSize(git.GetBranch().c_str()).x;
    float arrow_w = ImGui::GetFrameHeight();
    float combo_w = branch_text_w + arrow_w + ImGui::GetStyle().FramePadding.x * 2.0f + 6.0f;
    float max_combo_w = ImGui::GetContentRegionAvail().x - total_right_w - ImGui::GetFrameHeight() - 24.0f;
    if (combo_w > max_combo_w) combo_w = max_combo_w;

    ImGui::SetNextItemWidth(combo_w);
    if (ImGui::BeginCombo("##git_branch_combo", git.GetBranch().c_str())) {
        if (ImGui::Selectable("+ Create New Branch...")) {
            show_git_branch_modal_ = true;
        }
        ImGui::Separator();
        for (const auto& b : git.GetBranchList()) {
            bool is_selected = (b == git.GetBranch());
            if (ImGui::Selectable(b.c_str(), is_selected)) {
                if (!is_selected) {
                    std::string err;
                    if (git.CheckoutBranch(b, err)) {
                        tab_bar_.ReloadAllFromDisk();
                        ScanProjectFiles();
                        toast_manager_.ShowSuccess("Git: Switched to branch '" + b + "'");
                    } else {
                        toast_manager_.ShowError(err);
                    }
                }
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Branch: %s (click to switch)", git.GetBranch().c_str());

    ImGui::SameLine();
    if (ImGui::Button("+##git_new_branch")) {
        show_git_branch_modal_ = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create New Branch...");

    float right_x = ImGui::GetWindowContentRegionMax().x - total_right_w - 4.0f;
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
            git.RefreshAsync();
        }
    } else {
        if (ImGui::SmallButton("↻##git_refresh")) {
            git.RefreshAsync();
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh Git Status");

    ImGui::SameLine();
    if (ImGui::SmallButton("···##git_more_actions")) {
        ImGui::OpenPopup("##git_more_actions_popup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("More Actions...");

    ImGui::PopStyleColor(3);

    // ── More Actions (...) Menu ─────────────────────────────────────────
    if (ImGui::BeginPopup("##git_more_actions_popup")) {
        if (ImGui::MenuItem("View as Tree", nullptr, git_view_as_tree_)) {
            git_view_as_tree_ = !git_view_as_tree_;
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Pull")) {
            std::string err;
            if (git.Pull(err)) {
                tab_bar_.ReloadAllFromDisk();
                ScanProjectFiles();
                toast_manager_.ShowSuccess("Git: Pull completed.");
            } else {
                toast_manager_.ShowError("Git Pull failed: " + err);
            }
        }
        if (ImGui::MenuItem("Push")) {
            if (!git.HasRemote("origin")) {
                show_git_remote_modal_ = true;
            } else {
                std::string err;
                if (git.Push(false, err)) {
                    toast_manager_.ShowSuccess("Git: Changes pushed successfully.");
                } else {
                    toast_manager_.ShowError("Git Push failed: " + err);
                }
            }
        }
        if (ImGui::MenuItem("Clone...")) {
            show_git_clone_modal_ = true;
        }
        if (ImGui::MenuItem("Checkout to...")) {
            show_git_branch_modal_ = true;
        }
        if (ImGui::MenuItem("Fetch")) {
            std::string err;
            if (git.Fetch(err)) {
                toast_manager_.ShowSuccess("Git: Fetch completed.");
            } else {
                toast_manager_.ShowError("Git Fetch failed: " + err);
            }
        }
        ImGui::Separator();

        if (ImGui::BeginMenu("Commit")) {
            if (ImGui::MenuItem("Commit Staged (Amend)")) {
                RequestConfirmation("Commit (Amend)",
                    "Are you sure you want to amend the previous commit?\nThis will modify the last commit with current staged changes.",
                    "Amend Commit", ImVec4(0.2f, 0.55f, 0.85f, 1.0f),
                    [this]() {
                        std::string err;
                        if (GitManager::Instance().CommitAmend("", err)) {
                            tab_bar_.ReloadAllFromDisk();
                            toast_manager_.ShowSuccess("Git: Commit amended successfully.");
                        } else {
                            toast_manager_.ShowError(err);
                        }
                    });
            }
            if (ImGui::MenuItem("Undo Last Commit (Soft)")) {
                RequestConfirmation("Undo Last Commit",
                    "Are you sure you want to undo the last commit?\nChanges will be preserved in your working tree.",
                    "Undo Commit", ImVec4(0.85f, 0.4f, 0.1f, 1.0f),
                    [this]() {
                        std::string err;
                        if (GitManager::Instance().UndoLastCommit(true, err)) {
                            tab_bar_.ReloadAllFromDisk();
                            toast_manager_.ShowSuccess("Git: Undid last commit (kept staged).");
                        } else {
                            toast_manager_.ShowError(err);
                        }
                    });
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Changes")) {
            if (ImGui::MenuItem("Stage All Changes")) {
                git.StageAll();
                toast_manager_.ShowSuccess("Git: Staged all changes.");
            }
            if (ImGui::MenuItem("Unstage All Changes")) {
                git.UnstageAll();
                toast_manager_.ShowInfo("Git: Unstaged all changes.");
            }
            if (ImGui::MenuItem("Discard All Changes")) {
                RequestConfirmation("Discard All Changes",
                    "Are you sure you want to discard ALL unstaged changes?\nThis action CANNOT be undone.",
                    "Discard All", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
                    [this]() {
                        std::string err;
                        if (GitManager::Instance().DiscardAll(err)) {
                            tab_bar_.ReloadAllFromDisk();
                            toast_manager_.ShowInfo("Git: Discarded all changes.");
                        } else {
                            toast_manager_.ShowError(err);
                        }
                    });
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Pull, Push")) {
            if (ImGui::MenuItem("Pull")) {
                std::string err;
                if (git.Pull(err)) {
                    tab_bar_.ReloadAllFromDisk();
                    ScanProjectFiles();
                    toast_manager_.ShowSuccess("Git: Pull completed.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            if (ImGui::MenuItem("Push")) {
                std::string err;
                if (git.Push(false, err)) {
                    toast_manager_.ShowSuccess("Git: Push completed.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            if (ImGui::MenuItem("Push (Force)")) {
                RequestConfirmation("Force Push",
                    "WARNING: Force pushing will overwrite remote branch history!\nAre you sure you want to proceed?",
                    "Force Push", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
                    [this]() {
                        std::string err;
                        if (GitManager::Instance().PushForce(err)) {
                            toast_manager_.ShowSuccess("Git: Force push completed.");
                        } else {
                            toast_manager_.ShowError(err);
                        }
                    });
            }
            if (ImGui::MenuItem("Push (Tags)")) {
                std::string err;
                if (git.PushTags(err)) {
                    toast_manager_.ShowSuccess("Git: Pushed tags.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Branch")) {
            if (ImGui::MenuItem("Switch Branch...")) {
                show_git_branch_modal_ = true;
            }
            if (ImGui::MenuItem("Create Branch...")) {
                show_git_branch_modal_ = true;
            }
            if (ImGui::MenuItem("Rename Branch...")) {
                show_git_branch_modal_ = true;
            }
            if (ImGui::MenuItem("Delete Branch...")) {
                show_git_branch_modal_ = true;
            }
            if (ImGui::MenuItem("Merge Branch...")) {
                show_git_branch_modal_ = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Remote")) {
            if (ImGui::MenuItem("Add Remote...")) {
                show_git_remote_modal_ = true;
            }
            if (ImGui::MenuItem("Manage Remotes...")) {
                show_git_remote_modal_ = true;
            }
            if (ImGui::MenuItem("Fetch (Prune)")) {
                std::string err;
                if (git.FetchPrune(err)) {
                    toast_manager_.ShowSuccess("Git: Fetched and pruned remotes.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Stash")) {
            if (ImGui::MenuItem("Stash (Include Untracked)")) {
                std::string err;
                if (git.StashSave("", true, false, err)) {
                    tab_bar_.ReloadAllFromDisk();
                    toast_manager_.ShowSuccess("Git: Stashed changes (including untracked).");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            if (ImGui::MenuItem("Stash (Keep Staged)")) {
                std::string err;
                if (git.StashSave("", true, true, err)) {
                    tab_bar_.ReloadAllFromDisk();
                    toast_manager_.ShowSuccess("Git: Stashed changes (kept staged).");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            if (ImGui::MenuItem("Pop Latest Stash")) {
                std::string err;
                if (git.StashPop(0, err)) {
                    tab_bar_.ReloadAllFromDisk();
                    toast_manager_.ShowSuccess("Git: Popped latest stash.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            if (ImGui::MenuItem("Apply Latest Stash")) {
                std::string err;
                if (git.StashApply(0, err)) {
                    tab_bar_.ReloadAllFromDisk();
                    toast_manager_.ShowSuccess("Git: Applied latest stash.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            if (ImGui::MenuItem("View / Manage Stashes...")) {
                show_git_stash_modal_ = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tags")) {
            if (ImGui::MenuItem("Create Tag...")) {
                show_git_tags_modal_ = true;
            }
            if (ImGui::MenuItem("Manage Tags...")) {
                show_git_tags_modal_ = true;
            }
            if (ImGui::MenuItem("Push Tags")) {
                std::string err;
                if (git.PushTags(err)) {
                    toast_manager_.ShowSuccess("Git: Tags pushed.");
                } else {
                    toast_manager_.ShowError(err);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Show Git Output")) {
            show_git_output_modal_ = true;
        }

        ImGui::EndPopup();
    }

    ImGui::Spacing();

    // ── Sync Actions (Push & Pull) ──────────────────────────────────────
    int ahead = git.GetAheadCount();
    int behind = git.GetBehindCount();
    std::string push_btn_label = ahead > 0 ? ("Push  ↑" + std::to_string(ahead)) : "Push";
    std::string pull_btn_label = behind > 0 ? ("Pull  ↓" + std::to_string(behind)) : "Pull";

    float avail_sync_w = ImGui::GetContentRegionAvail().x;
    float sync_btn_w = (avail_sync_w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (ImGui::Button(push_btn_label.c_str(), ImVec2(sync_btn_w, 0))) {
        if (!git.HasRemote("origin")) {
            show_git_remote_modal_ = true;
        } else {
            std::string err;
            if (git.Push(false, err)) {
                toast_manager_.ShowSuccess("Git: Changes pushed successfully.");
            } else {
                toast_manager_.ShowError("Git Push failed: " + err);
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        if (!git.HasRemote("origin")) {
            ImGui::SetTooltip("Publish Branch: Configure remote repository and push");
        } else {
            ImGui::SetTooltip("Push local commits to remote (%d unpushed)", ahead);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(pull_btn_label.c_str(), ImVec2(sync_btn_w, 0))) {
        if (!git.HasRemote("origin")) {
            show_git_remote_modal_ = true;
        } else {
            std::string err;
            if (git.Pull(err)) {
                tab_bar_.ReloadAllFromDisk();
                ScanProjectFiles();
                toast_manager_.ShowSuccess("Git: Pull completed successfully.");
            } else {
                toast_manager_.ShowError("Git Pull failed: " + err);
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Pull commits from remote (%d behind)", behind);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Staged list for commit button state & section
    const auto staged = git.GetStagedChanges();
    bool has_staged = !staged.empty();

    // Commit message input
    static char commit_msg[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##commit_msg", "Message (Ctrl+Enter to commit)", commit_msg, sizeof(commit_msg));

    bool trigger_commit = false;
    if (ImGui::IsItemFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        trigger_commit = true;
    }

    ImGui::Spacing();
    if (!has_staged) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Commit", ImVec2(-1, 0)) || (has_staged && trigger_commit)) {
        if (strlen(commit_msg) > 0) {
            std::string err;
            if (git.Commit(commit_msg, err)) {
                commit_msg[0] = '\0';
                tab_bar_.ReloadAllFromDisk();
                toast_manager_.ShowSuccess("Git: Changes committed successfully.");
            } else {
                toast_manager_.ShowError(err.empty() ? "Git: Commit failed." : err);
            }
        } else {
            toast_manager_.ShowWarning("Git: Please enter a commit message.");
        }
    }
    if (!has_staged) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("No staged changes to commit.\nStage files first using '+' next to a file or 'Stage All Changes'.");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Staged Changes ──────────────────────────────────────────────────
    if (!staged.empty()) {
        std::string staged_header = "STAGED CHANGES (" + std::to_string(staged.size()) + ")";
        ImGui::SetNextItemAllowOverlap();
        bool staged_open = ImGui::CollapsingHeader(staged_header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

        float action_w = 20.0f;
        float right_pos = ImGui::GetWindowContentRegionMax().x - action_w - 4.0f;
        if (right_pos > ImGui::GetCursorPosX()) ImGui::SameLine(right_pos);
        if (ImGui::SmallButton("-##unstage_all")) {
            git.UnstageAll();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unstage All Changes");

        if (staged_open) {
            for (const auto& item : staged) {
                ImGui::PushID(item.path.c_str());
                
                ImVec4 col = (item.type == GitStatusType::Added) ? ImVec4(0.45f, 0.79f, 0.57f, 1.0f) :
                             (item.type == GitStatusType::Deleted) ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f) :
                             ImVec4(0.89f, 0.75f, 0.55f, 1.0f);
                char code = (item.type == GitStatusType::Added) ? 'A' :
                            (item.type == GitStatusType::Deleted) ? 'D' : 'M';

                ImGui::TextColored(col, "%c", code);
                ImGui::SameLine();

                float item_btn_w = 64.0f;
                float item_right = ImGui::GetWindowContentRegionMax().x - item_btn_w - 4.0f;
                float sel_w = item_right - ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x;
                if (sel_w < 10.0f) sel_w = 10.0f;

                std::string display_name = item.path;
                if (git_view_as_tree_) {
                    auto slash = item.path.find_last_of("/\\");
                    if (slash != std::string::npos) {
                        display_name = item.path.substr(slash + 1) + " (" + item.path.substr(0, slash) + ")";
                    }
                }

                if (ImGui::Selectable(display_name.c_str(), false, 0, ImVec2(sel_w, 0))) {
                    std::string full_path = git.GetRepoPath() + "/" + item.path;
                    tab_bar_.OpenFile(full_path, &theme_manager_.Active());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", item.path.c_str());

                ImGui::SameLine(item_right);
                if (ImGui::SmallButton("Diff##staged_diff")) {
                    ShowGitDiffModal(item.path);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("View Diff");

                ImGui::SameLine();
                if (ImGui::SmallButton("-##unstage_one")) {
                    git.UnstageFile(item.path);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unstage");

                ImGui::PopID();
            }
        }
    }

    // ── Changes (Working Tree) ──────────────────────────────────────────
    const auto changes = git.GetUnstagedChanges();
    std::string changes_header = "CHANGES (" + std::to_string(changes.size()) + ")";
    ImGui::SetNextItemAllowOverlap();
    bool changes_open = ImGui::CollapsingHeader(changes_header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
    if (!changes.empty()) {
        float action_w = 20.0f;
        float right_pos = ImGui::GetWindowContentRegionMax().x - action_w - 4.0f;
        if (right_pos > ImGui::GetCursorPosX()) ImGui::SameLine(right_pos);
        if (ImGui::SmallButton("+##stage_all")) {
            git.StageAll();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stage All Changes");
    }

    if (changes_open) {
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

                float item_btns_w = 88.0f;
                float item_right = ImGui::GetWindowContentRegionMax().x - item_btns_w - 4.0f;
                float sel_w = item_right - ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x;
                if (sel_w < 10.0f) sel_w = 10.0f;

                std::string display_name = item.path;
                if (git_view_as_tree_) {
                    auto slash = item.path.find_last_of("/\\");
                    if (slash != std::string::npos) {
                        display_name = item.path.substr(slash + 1) + " (" + item.path.substr(0, slash) + ")";
                    }
                }

                if (ImGui::Selectable(display_name.c_str(), false, 0, ImVec2(sel_w, 0))) {
                    std::string full_path = git.GetRepoPath() + "/" + item.path;
                    tab_bar_.OpenFile(full_path, &theme_manager_.Active());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", item.path.c_str());

                ImGui::SameLine(item_right);
                if (ImGui::SmallButton("Diff##change_diff")) {
                    ShowGitDiffModal(item.path);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("View Diff");

                ImGui::SameLine();
                if (ImGui::SmallButton("+##stage_one")) {
                    git.StageFile(item.path);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stage");

                ImGui::SameLine();
                if (ImGui::SmallButton("↺##discard_one")) {
                    std::string file_path = item.path;
                    RequestConfirmation("Discard File Changes",
                        "Are you sure you want to discard changes in:\n" + file_path + "\n\nThis action CANNOT be undone.",
                        "Discard", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
                        [file_path]() {
                            GitManager::Instance().DiscardChanges(file_path);
                        });
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

    // Git branch (if in a repo) - interactive clickable button
    if (GitManager::Instance().HasRepo()) {
        std::string branch_str = "  git: " + GitManager::Instance().GetBranch();
        int ahead = GitManager::Instance().GetAheadCount();
        int behind = GitManager::Instance().GetBehindCount();
        if (ahead > 0 || behind > 0) {
            branch_str += " (";
            if (ahead > 0) branch_str += "↑" + std::to_string(ahead);
            if (behind > 0) branch_str += (ahead > 0 ? " ↓" : "↓") + std::to_string(behind);
            branch_str += ")";
        }
        branch_str += "  ";

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.75f, 1.0f, 0.2f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.75f, 1.0f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.75f, 1.0f, 1.0f));

        if (ImGui::SmallButton(branch_str.c_str())) {
            show_git_branch_modal_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Branch: %s\nClick to switch or create branch", GitManager::Instance().GetBranch().c_str());
        }

        ImGui::PopStyleColor(4);
        ImGui::SameLine();
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
    command_palette_.RegisterCommand({"file.search_project", "Search in Project Files (Find in Files)", "Ctrl+Shift+F", [this]() {
        command_palette_.Open(PaletteMode::ProjectSearch);
    }});
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
            GitManager::Instance().SetRepoPath(folder);
            SaveSession();
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
            GitManager::Instance().RefreshAsync();
        }
    }});
    command_palette_.RegisterCommand({"view.toggle_split", "View: Toggle Split Editor", "Ctrl+\\", [this]() {
        tab_bar_.ToggleSplitView();
    }});
    command_palette_.RegisterCommand({"git.refresh", "Git: Refresh Status", "", [this]() {
        GitManager::Instance().RefreshAsync();
        toast_manager_.ShowInfo("Git: Status refreshed.");
    }});
    command_palette_.RegisterCommand({"git.branch.switch", "Git: Switch / Checkout Branch...", "", [this]() {
        show_git_branch_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.branch.create", "Git: Create New Branch...", "", [this]() {
        show_git_branch_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.push", "Git: Push", "", [this]() {
        auto& git = GitManager::Instance();
        if (!git.HasRemote("origin")) {
            show_git_remote_modal_ = true;
        } else {
            std::string err;
            if (git.Push(false, err)) {
                toast_manager_.ShowSuccess("Git: Changes pushed successfully.");
            } else {
                toast_manager_.ShowError("Git Push failed: " + err);
            }
        }
    }});
    command_palette_.RegisterCommand({"git.pull", "Git: Pull", "", [this]() {
        auto& git = GitManager::Instance();
        if (!git.HasRemote("origin")) {
            show_git_remote_modal_ = true;
        } else {
            std::string err;
            if (git.Pull(err)) {
                toast_manager_.ShowSuccess("Git: Pull completed successfully.");
            } else {
                toast_manager_.ShowError("Git Pull failed: " + err);
            }
        }
    }});
    command_palette_.RegisterCommand({"git.remote.add", "Git: Add Remote Repository...", "", [this]() {
        show_git_remote_modal_ = true;
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
    command_palette_.RegisterCommand({"git.discard_all", "Git: Discard All Changes", "", [this]() {
        RequestConfirmation("Discard All Changes",
            "Are you sure you want to discard ALL unstaged changes?\nThis action CANNOT be undone.",
            "Discard All", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
            [this]() {
                std::string err;
                if (GitManager::Instance().DiscardAll(err)) {
                    tab_bar_.ReloadAllFromDisk();
                    toast_manager_.ShowInfo("Git: Discarded all changes.");
                } else {
                    toast_manager_.ShowError(err);
                }
            });
    }});
    command_palette_.RegisterCommand({"git.push_force", "Git: Push (Force)", "", [this]() {
        RequestConfirmation("Force Push",
            "WARNING: Force pushing will overwrite remote branch history!\nAre you sure you want to proceed?",
            "Force Push", ImVec4(0.85f, 0.25f, 0.25f, 1.0f),
            [this]() {
                std::string err;
                if (GitManager::Instance().PushForce(err)) {
                    toast_manager_.ShowSuccess("Git: Force push completed.");
                } else {
                    toast_manager_.ShowError(err);
                }
            });
    }});
    command_palette_.RegisterCommand({"git.clone", "Git: Clone Repository...", "", [this]() {
        show_git_clone_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.branch.manage", "Git: Manage Branches...", "", [this]() {
        show_git_branch_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.remote.manage", "Git: Manage Remotes...", "", [this]() {
        show_git_remote_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.stash.save", "Git: Stash (Include Untracked)", "", [this]() {
        std::string err;
        if (GitManager::Instance().StashSave("", true, false, err)) {
            tab_bar_.ReloadAllFromDisk();
            toast_manager_.ShowSuccess("Git: Stashed changes.");
        } else {
            toast_manager_.ShowError(err);
        }
    }});
    command_palette_.RegisterCommand({"git.stash.pop", "Git: Pop Latest Stash", "", [this]() {
        std::string err;
        if (GitManager::Instance().StashPop(0, err)) {
            tab_bar_.ReloadAllFromDisk();
            toast_manager_.ShowSuccess("Git: Popped latest stash.");
        } else {
            toast_manager_.ShowError(err);
        }
    }});
    command_palette_.RegisterCommand({"git.stash.manage", "Git: Manage Stashes...", "", [this]() {
        show_git_stash_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.tag.manage", "Git: Manage Tags...", "", [this]() {
        show_git_tags_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.output", "Git: Show Git Output Log", "", [this]() {
        show_git_output_modal_ = true;
    }});
    command_palette_.RegisterCommand({"git.view_diff", "Git: View File Diff", "", [this]() {
        if (auto* tab = tab_bar_.ActiveTab()) {
            if (!tab->filepath.empty()) {
                ShowGitDiffModal(tab->filepath);
            }
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

    command_palette_.RegisterCommand({"view.toggle_minimap", "View: Toggle Minimap", "Ctrl+M", [this]() {
        ToggleMinimap();
    }});

    for (auto& name : theme_manager_.GetThemeNames()) {
        command_palette_.RegisterCommand({"theme." + name, "Theme: " + name, "", [this, name]() {
            theme_manager_.SetTheme(name);
            SaveSession();
        }});
    }

    command_palette_.RegisterCommand({"theme.reload", "Theme: Reload Custom Themes", "", [this]() {
        theme_manager_.ReloadThemes();
        // Note: For newly added themes to appear in the palette without restarting,
        // we'd need to clear and re-register commands. For now, this just reloads the CSS 
        // files and applies any changes to the CURRENT custom theme instantly.
    }});

    command_palette_.RegisterCommand({"editor.goto_definition", "Editor: Go to Definition", "F12 / Ctrl+Enter", [this]() {
        if (auto* editor = tab_bar_.ActiveEditor()) {
            editor->GoToDefinition();
        }
    }});
}

// ── Project file scanning ─────────────────────────────────────────────────

/// Recursively scan the project root for files (used by Quick Open).
void App::ScanProjectFiles() {
    std::vector<std::string> files;
    std::string root = file_explorer_.Root();
    if (root.empty()) return;

    std::error_code ec;
    auto iter = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
    for (const auto& entry : iter) {
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
    command_palette_.SetProjectRoot(root);
    symbol_index_.Clear();
    symbol_index_.IndexDirectoryAsync(root);
}

void App::SetMinimapEnabled(bool enabled) {
    show_minimap_ = enabled;
    tab_bar_.SetMinimapEnabled(show_minimap_);
    SaveSession();
}

void App::LoadSession() {
    std::string config_path = platform::GetExecutableDir() + "/session.json";
    if (!fs::exists(config_path)) return;

    std::ifstream file(config_path);
    if (!file.is_open()) return;

    try {
        json j;
        file >> j;

        if (j.contains("theme") && j["theme"].is_string()) {
            std::string th_name = j["theme"].get<std::string>();
            theme_manager_.SetTheme(th_name);
        }

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

        if (j.contains("show_minimap") && j["show_minimap"].is_boolean()) {
            show_minimap_ = j["show_minimap"].get<bool>();
            tab_bar_.SetMinimapEnabled(show_minimap_);
        }

        if (j.contains("show_terminal") && j["show_terminal"].is_boolean()) {
            show_terminal_ = j["show_terminal"].get<bool>();
        }

        if (j.contains("show_file_explorer") && j["show_file_explorer"].is_boolean()) {
            show_file_explorer_ = j["show_file_explorer"].get<bool>();
        }

        if (j.contains("show_source_control") && j["show_source_control"].is_boolean()) {
            show_source_control_ = j["show_source_control"].get<bool>();
        }

        if (j.contains("tabs") && j["tabs"].is_array()) {
            for (const auto& item : j["tabs"]) {
                if (item.is_object() && item.contains("path") && item["path"].is_string()) {
                    std::string f = item["path"].get<std::string>();
                    if (fs::exists(f)) {
                        tab_bar_.OpenFile(f, &theme_manager_.Active());
                        if (auto* ed = tab_bar_.ActiveEditor()) {
                            int line = item.value("line", 0);
                            int col = item.value("col", 0);
                            ed->GoToLine(line);
                            if (!ed->GetCursors().cursors.empty()) {
                                ed->GetCursors().Primary().position.column = col;
                            }
                        }
                    }
                }
            }
        } else if (j.contains("files") && j["files"].is_array()) {
            for (const auto& item : j["files"]) {
                if (item.is_string()) {
                    std::string f = item.get<std::string>();
                    if (fs::exists(f)) {
                        tab_bar_.OpenFile(f, &theme_manager_.Active());
                    }
                }
            }
        }

        if (j.contains("active_tab") && j["active_tab"].is_number_integer()) {
            tab_bar_.SetActiveIndex(j["active_tab"].get<int>());
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
    j["theme"] = theme_manager_.Active().name;
    j["show_minimap"] = show_minimap_;
    j["show_terminal"] = show_terminal_;
    j["show_file_explorer"] = show_file_explorer_;
    j["show_source_control"] = show_source_control_;
    j["active_tab"] = tab_bar_.ActiveIndex();

    json tabs_arr = json::array();
    for (const auto& tab : tab_bar_.GetTabs()) {
        if (tab && !tab->filepath.empty()) {
            json tab_obj;
            tab_obj["path"] = tab->filepath;
            const auto& cursors = tab->editor.GetCursors();
            if (!cursors.cursors.empty()) {
                tab_obj["line"] = cursors.Primary().position.line;
                tab_obj["col"] = cursors.Primary().position.column;
            } else {
                tab_obj["line"] = 0;
                tab_obj["col"] = 0;
            }
            tabs_arr.push_back(tab_obj);
        }
    }
    j["tabs"] = tabs_arr;

    file << j.dump(4);
}

void App::OnFileDrop(const std::string& path) {
    if (fs::is_directory(path)) {
        file_explorer_.SetRoot(path);
        terminal_.SetWorkingDirectory(path);
        ScanProjectFiles();
        GitManager::Instance().SetRepoPath(path);
        SaveSession();
    } else {
        tab_bar_.OpenFile(path, &theme_manager_.Active());
        SaveSession();
    }
}

void App::OnFocusGained() {
    tab_bar_.ReloadAllFromDisk();
    if (GitManager::Instance().HasRepo()) {
        GitManager::Instance().RefreshAsync();
    }
}

}  // namespace luce
