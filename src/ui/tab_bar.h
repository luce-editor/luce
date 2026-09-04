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
    MarkdownPreview                 markdown_preview;
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

private:
    std::vector<std::unique_ptr<Tab>> tabs_;
    int active_ = -1;
};

}  // namespace luce
