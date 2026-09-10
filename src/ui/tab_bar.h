#pragma once
// ============================================================================
// TabBar — Manages open file tabs with dirty indicators and context menus.
// ============================================================================

#include "editor/editor_view.h"
#include "editor/text_buffer.h"
#include "syntax/syntax_highlighter.h"
#include "ui/markdown_preview.h"
#include "ui/theme.h"

#include <memory>
#include <string>
#include <vector>

namespace luce {

/// Represents a single open document (tab).
struct Tab {
    std::string                     filepath;      ///< Absolute path (or empty for untitled).
    std::string                     title;         ///< Display name in the tab bar.
    std::unique_ptr<TextBuffer>     buffer;
    std::unique_ptr<SyntaxHighlighter> highlighter;
    EditorView                      editor;
    EditorView                      split_editor;  ///< Independent view for right split pane (scroll, cursor, selection)
    MarkdownPreview                 markdown_preview;
    MarkdownPreview                 split_markdown_preview;
    bool                            show_markdown_preview = false;
    bool                            is_image = false;
    unsigned int                    image_texture = 0;
    int                             image_width = 0;
    int                             image_height = 0;
};

/// Manages the collection of open tabs and renders the tab bar UI.
class TabBar {
public:
    TabBar();

    /// Open a file in a new tab (or switch to it if already open).
    void OpenFile(const std::string& path, const Theme* theme);

    /// Create a new untitled tab.
    void NewFile(const Theme* theme);

    /// Save the active tab to disk.  Returns false on error.
    bool SaveActive();

    /// Save the active tab to a new path.
    bool SaveActiveAs(const std::string& path);

    /// Close a tab by index.  Returns false if the user should be
    /// prompted to save first (dirty buffer).
    bool CloseTab(int index);

    /// Toggle Markdown Preview for the active tab (Ctrl+Shift+M).
    void ToggleActiveMarkdownPreview();

    /// Render the tab bar and the active editor.
    void Render(const Theme* theme, ImFont* editor_font = nullptr,
                ImFont* bold_font = nullptr, ImFont* italic_font = nullptr,
                ImFont* h1_font = nullptr, ImFont* h2_font = nullptr);

    /// Reload all non-dirty open tabs from disk (e.g. after branch switch or external change).
    void ReloadAllFromDisk();

    /// Reload a specific tab from disk if it is not dirty.
    bool ReloadTab(int index);

    // ── Accessors ─────────────────────────────────────────────────────────

    Tab*        ActiveTab();
    int         ActiveIndex() const    { return active_; }
    int         TabCount() const       { return static_cast<int>(tabs_.size()); }
    const std::vector<std::unique_ptr<Tab>>& GetTabs() const { return tabs_; }
    bool        HasUnsaved() const;
    EditorView* ActiveEditor();

    /// Switch to the next / previous tab.
    void NextTab();
    void PrevTab();

    void SetActiveIndex(int idx) {
        if (idx >= 0 && idx < static_cast<int>(tabs_.size())) {
            active_ = idx;
            tab_to_select_ = idx;
        }
    }

    // ── Split View & Drag-Drop ───────────────────────────────────────────
    void ToggleSplitView();
    bool IsSplitView() const { return split_view_; }
    void SetSplitView(bool enabled);
    int  SplitActiveIndex() const { return split_active_; }
    void SetSplitActiveIndex(int idx);

    /// Drag & Drop helpers
    void MoveTab(int from_idx, int to_idx);
    void SplitTabToSide(int tab_idx, const Theme* theme = nullptr);
    void OpenFileToSide(const std::string& path, const Theme* theme);
    void MoveTabToPane(int tab_idx, int pane_idx);

    // ── Minimap & Symbol Navigation ───────────────────────────────────────
    void SetMinimapEnabled(bool enabled);
    bool IsMinimapEnabled() const { return show_minimap_; }

    void SetSymbolIndex(class SymbolIndex* index);
    void SetOnGoToDefinition(std::function<void(const std::string&, int)> cb);

    // ── Plugin Lifecycle & Event Hooks ────────────────────────────────────
    void SetOnFileOpened(std::function<void(const std::string&)> cb) { on_file_opened_ = std::move(cb); }
    void SetOnBeforeSave(std::function<void(const std::string&)> cb) { on_before_save_ = std::move(cb); }
    void SetOnAfterSave(std::function<void(const std::string&)> cb) { on_after_save_ = std::move(cb); }
    void SetOnTextChanged(std::function<void(int, int)> cb) { on_text_changed_ = std::move(cb); }
    void SetCompletionProvider(std::function<std::vector<std::string>(const std::string&, const std::string&, int, int)> cb) {
        completion_provider_ = std::move(cb);
        for (auto& tab : tabs_) {
            if (tab) {
                tab->editor.SetCompletionProvider(completion_provider_);
                tab->split_editor.SetCompletionProvider(completion_provider_);
            }
        }
    }

private:
    std::vector<std::unique_ptr<Tab>> tabs_;
    int active_ = -1;
    int tab_to_select_ = -1;
    bool split_view_ = false;
    int split_active_ = -1;
    int focused_pane_ = 0; // 0 = left, 1 = right
    bool show_minimap_ = true;
    class SymbolIndex* symbol_index_ = nullptr;
    std::function<void(const std::string&, int)> on_goto_definition_;

    std::function<void(const std::string&)> on_file_opened_;
    std::function<void(const std::string&)> on_before_save_;
    std::function<void(const std::string&)> on_after_save_;
    std::function<void(int, int)> on_text_changed_;
    std::function<std::vector<std::string>(const std::string&, const std::string&, int, int)> completion_provider_;

    // Drop target overlays & handlers
    void RenderDropOverlay(const ImVec2& min_pos, const ImVec2& max_pos, const char* title, const char* subtitle, bool hovered);
    void HandleEditorDropTargets(const Theme* theme, const ImVec2& canvas_min, const ImVec2& canvas_max);
    void HandleSplitPaneDropTargets(const Theme* theme, const ImVec2& left_min, const ImVec2& left_max, const ImVec2& right_min, const ImVec2& right_max);
};

}  // namespace luce
