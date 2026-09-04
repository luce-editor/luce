#pragma once
// ============================================================================
// TextBuffer — Line-based text buffer with undo / redo support.
//
// Stores the document as a vector of UTF-8 strings (one per line).
// All mutations go through atomic "edit actions" that are recorded on an
// undo stack so they can be reversed.
//
// The interface is deliberately simple; higher-level features (multi-cursor
// editing, auto-indent) live in EditorView and delegate to this class.
// ============================================================================

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace luce {

/// Callback signature for notifying observers that specific lines changed.
/// Parameters: (first_changed_line, number_of_lines_affected).
using BufferChangeCallback = std::function<void(int, int)>;

class TextBuffer {
public:
    TextBuffer();

    // ── Content access ────────────────────────────────────────────────────

    int                GetLineCount() const;
    const std::string& GetLine(int index) const;
    std::string        GetText() const;       ///< Return the full document.
    bool               IsEmpty() const;

    // ── Mutation ──────────────────────────────────────────────────────────

    /// Insert `text` at (line, column).  The text may contain newlines.
    void InsertText(int line, int column, const std::string& text);

    /// Delete the range [start_line, start_col) .. [end_line, end_col).
    void DeleteRange(int start_line, int start_col, int end_line, int end_col);

    /// Replace the entire buffer with `text`.
    void SetText(const std::string& text);

    // ── Undo / Redo ──────────────────────────────────────────────────────

    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();

    /// Group subsequent edits into a single undo step.
    void BeginUndoGroup();
    void EndUndoGroup();

    // ── File I/O ─────────────────────────────────────────────────────────

    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;

    // ── Dirty tracking ───────────────────────────────────────────────────

    bool IsDirty() const       { return dirty_; }
    void ClearDirty()          { dirty_ = false; }

    // ── Observer ─────────────────────────────────────────────────────────

    /// Register a callback that fires whenever lines change (used to
    /// invalidate the syntax-highlight cache).
    void SetChangeCallback(BufferChangeCallback cb) { on_change_ = std::move(cb); }

private:
    // ── Internal edit actions (used for undo/redo) ─────────────────────

    struct InsertAction {
        int         line, column;
        std::string text;
    };

    struct DeleteAction {
        int         start_line, start_col;
        int         end_line,   end_col;
        std::string deleted_text;
    };

    using EditAction = std::variant<InsertAction, DeleteAction>;

    struct UndoGroup {
        std::vector<EditAction> actions;
    };

    // Perform an insert without recording undo.
    void RawInsert(int line, int column, const std::string& text);
    // Perform a delete without recording undo, returns deleted text.
    std::string RawDelete(int start_line, int start_col, int end_line, int end_col);

    void NotifyChange(int line, int count);

    std::vector<std::string>  lines_;
    std::vector<UndoGroup>    undo_stack_;
    std::vector<UndoGroup>    redo_stack_;
    bool                      in_undo_group_ = false;
    UndoGroup                 current_group_;
    bool                      dirty_         = false;
    BufferChangeCallback      on_change_;
};

}  // namespace luce
