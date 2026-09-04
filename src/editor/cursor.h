#pragma once
// ============================================================================
// Cursor — Position and selection primitives for the text editor.
// ============================================================================

#include <algorithm>
#include <vector>

namespace luce {

/// A position in the text buffer identified by (line, column).
/// Both fields are 0-based.
struct TextPosition {
    int line   = 0;
    int column = 0;

    bool operator==(const TextPosition& o) const { return line == o.line && column == o.column; }
    bool operator!=(const TextPosition& o) const { return !(*this == o); }

    /// Lexicographic ordering: earlier in the document compares less.
    bool operator<(const TextPosition& o) const {
        return (line < o.line) || (line == o.line && column < o.column);
    }
    bool operator<=(const TextPosition& o) const { return !(o < *this); }
    bool operator>(const TextPosition& o) const  { return o < *this; }
    bool operator>=(const TextPosition& o) const { return !(*this < o); }
};

/// A single cursor with an optional selection range.
///
/// `position` is where the caret is drawn (the "head").
/// `selection_start` is the anchor — the point where the user began selecting.
/// When there is no selection, `position == selection_start`.
struct Cursor {
    TextPosition position;
    TextPosition selection_start;

    /// Returns true when there is an active selection.
    bool HasSelection() const { return position != selection_start; }

    /// Returns the earlier of position / selection_start.
    TextPosition SelectionBegin() const { return std::min(position, selection_start); }

    /// Returns the later of position / selection_start.
    TextPosition SelectionEnd() const { return std::max(position, selection_start); }

    /// Collapse the selection so that anchor == head.
    void ClearSelection() { selection_start = position; }

    /// Move the cursor to `pos`, extending the selection if `extend` is true.
    void MoveTo(TextPosition pos, bool extend = false) {
        position = pos;
        if (!extend) {
            selection_start = pos;
        }
    }
};

/// Container for zero or more cursors, enabling multi-cursor editing.
///
/// There is always at least one cursor (index 0 is the "primary" cursor).
/// After mutations that add or remove cursors the caller should call
/// `MergeDuplicates()` to ensure no two cursors occupy the same position.
struct CursorSet {
    std::vector<Cursor> cursors;

    CursorSet() { cursors.push_back(Cursor{}); }

    Cursor&       Primary()       { return cursors[0]; }
    const Cursor& Primary() const { return cursors[0]; }

    /// Add an additional cursor at the given position.
    void AddCursor(TextPosition pos) {
        Cursor c;
        c.position = pos;
        c.selection_start = pos;
        cursors.push_back(c);
    }

    /// Collapse back to a single primary cursor.
    void ResetToSingle() {
        Cursor primary = cursors[0];
        cursors.clear();
        cursors.push_back(primary);
    }

    /// Remove cursors that have identical positions (keep the first occurrence).
    void MergeDuplicates() {
        for (size_t i = 0; i < cursors.size(); ++i) {
            for (size_t j = i + 1; j < cursors.size();) {
                if (cursors[i].position == cursors[j].position) {
                    cursors.erase(cursors.begin() + static_cast<ptrdiff_t>(j));
                } else {
                    ++j;
                }
            }
        }
    }
};

}  // namespace luce
