#pragma once
// ============================================================================
// CommandPalette — Fuzzy-search command palette (Ctrl+Shift+P / Ctrl+P).
// ============================================================================

#include <functional>
#include <string>
#include <vector>

namespace luce {

struct Command {
    std::string id;           ///< Unique identifier (e.g. "editor.save").
    std::string display_name; ///< Human-readable name (e.g. "Save File").
    std::string shortcut;     ///< Optional shortcut hint (e.g. "Ctrl+S").
    std::function<void()> action;
};

/// Modes the palette can operate in.
enum class PaletteMode {
    Commands,      ///< Search through registered commands.
    Files,         ///< Quick open — search project files by name.
    GoToLine,      ///< Jump to a specific line number.
    ProjectSearch, ///< Search text across all project files.
};

struct SearchResult {
    std::string file_path;      ///< Relative file path for display
    std::string full_path;      ///< Full filesystem path
    int         line_number = 1;///< 1-indexed
    int         col_number = 1; ///< 1-indexed
    std::string line_content;   ///< Preview snippet of the matching line
};

class CommandPalette {
public:
    CommandPalette();

    /// Register a command.  The same command can be registered once.
    void RegisterCommand(Command cmd);

    /// Unregister all commands whose id starts with a prefix.
    void UnregisterCommandsWithPrefix(const std::string& prefix);

    /// Open the palette in the given mode.
    void Open(PaletteMode mode = PaletteMode::Commands);

    /// Close the palette.
    void Close();

    /// Current mode of the palette.
    PaletteMode GetMode() const { return mode_; }

    /// Switch palette mode and reset active query/selection state.
    void SetMode(PaletteMode mode);

    bool IsOpen() const { return open_; }

    /// Set the list of project files (used for Quick Open and Project Search).
    void SetProjectFiles(const std::vector<std::string>& files);

    /// Set root directory of current project (for resolving full paths).
    void SetProjectRoot(const std::string& root) { project_root_ = root; }

    /// Callback for when a file is opened via Quick Open.
    void SetOnOpenFile(std::function<void(const std::string&)> cb) { on_open_file_ = std::move(cb); }

    /// Callback for opening a file at a specific line and column (Project Search).
    void SetOnOpenFileAtLine(std::function<void(const std::string&, int, int)> cb) { on_open_file_at_line_ = std::move(cb); }

    /// Callback for Go to Line.
    void SetOnGoToLine(std::function<void(int)> cb) { on_go_to_line_ = std::move(cb); }

    /// Render the palette (call every frame).
    void Render();

private:
    /// Simple fuzzy match: returns true if all characters of `pattern`
    /// appear (in order) in `text`.
    bool FuzzyMatch(const std::string& text, const std::string& pattern) const;

    /// Perform project-wide text search across project_files_.
    void PerformProjectSearch(const std::string& query);

    bool                       open_ = false;
    PaletteMode                mode_ = PaletteMode::Commands;
    char                       input_buf_[256] = {};
    int                        focus_input_frames_ = 0;
    int                        selected_index_ = 0;

    std::vector<Command>       commands_;
    std::vector<std::string>   project_files_;
    std::string                project_root_;

    std::vector<SearchResult>  search_results_;
    std::string                last_search_query_;

    std::function<void(const std::string&)>             on_open_file_;
    std::function<void(const std::string&, int, int)>   on_open_file_at_line_;
    std::function<void(int)>                            on_go_to_line_;
};

}  // namespace luce
