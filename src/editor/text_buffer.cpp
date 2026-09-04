#include "text_buffer.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace luce {

TextBuffer::TextBuffer() {
    lines_.emplace_back();  // Start with a single empty line.
}

// ── Content access ────────────────────────────────────────────────────────

int TextBuffer::GetLineCount() const {
    return static_cast<int>(lines_.size());
}

const std::string& TextBuffer::GetLine(int index) const {
    static const std::string kEmpty;
    if (index < 0 || index >= GetLineCount()) return kEmpty;
    return lines_[index];
}

std::string TextBuffer::GetText() const {
    std::string result;
    for (int i = 0; i < GetLineCount(); ++i) {
        if (i > 0) result += '\n';
        result += lines_[i];
    }
    return result;
}

bool TextBuffer::IsEmpty() const {
    return lines_.size() == 1 && lines_[0].empty();
}

// ── Raw mutations (no undo recording) ─────────────────────────────────────

/// Insert `text` at (line, column) directly into the line buffer.
/// Handles embedded newlines by splitting/joining lines as needed.
void TextBuffer::RawInsert(int line, int column, const std::string& text) {
    if (line < 0 || line >= GetLineCount()) return;

    auto& current_line = lines_[line];
    column = std::clamp(column, 0, static_cast<int>(current_line.size()));

    // Split the text on newline boundaries.
    std::vector<std::string> new_lines;
    std::istringstream stream(text);
    std::string segment;
    bool first = true;
    while (std::getline(stream, segment, '\n')) {
        if (!first) new_lines.push_back(segment);
        else { new_lines.push_back(segment); first = false; }
    }
    // Handle trailing newline: std::getline won't produce an extra empty
    // string for a trailing '\n', but we need one.
    if (!text.empty() && text.back() == '\n') {
        new_lines.emplace_back();
    }

    if (new_lines.size() == 1) {
        // Simple in-line insertion.
        current_line.insert(column, new_lines[0]);
        NotifyChange(line, 1);
    } else {
        // Multi-line insertion: split the current line at `column`, then
        // weave in the new content.
        std::string tail = current_line.substr(column);
        current_line     = current_line.substr(0, column) + new_lines[0];

        // Insert intermediate and last lines.
        auto it = lines_.begin() + line + 1;
        for (size_t i = 1; i < new_lines.size(); ++i) {
            std::string l = new_lines[i];
            if (i == new_lines.size() - 1) l += tail;  // Append the tail to the last piece.
            it = lines_.insert(it, std::move(l));
            ++it;
        }
        NotifyChange(line, static_cast<int>(new_lines.size()));
    }
}

/// Delete the range [start_line:start_col .. end_line:end_col) and return
/// the deleted text.
std::string TextBuffer::RawDelete(int start_line, int start_col,
                                   int end_line,   int end_col) {
    if (start_line < 0 || end_line >= GetLineCount()) return {};
    if (start_line == end_line && start_col == end_col) return {};

    // Clamp columns.
    start_col = std::clamp(start_col, 0, static_cast<int>(lines_[start_line].size()));
    end_col   = std::clamp(end_col,   0, static_cast<int>(lines_[end_line].size()));

    // Collect the deleted text.
    std::string deleted;
    if (start_line == end_line) {
        deleted = lines_[start_line].substr(start_col, end_col - start_col);
        lines_[start_line].erase(start_col, end_col - start_col);
        NotifyChange(start_line, 1);
    } else {
        // First partial line.
        deleted = lines_[start_line].substr(start_col);
        std::string head = lines_[start_line].substr(0, start_col);

        // Middle full lines.
        for (int i = start_line + 1; i < end_line; ++i) {
            deleted += '\n';
            deleted += lines_[i];
        }

        // Last partial line.
        deleted += '\n';
        deleted += lines_[end_line].substr(0, end_col);
        std::string tail = lines_[end_line].substr(end_col);

        // Merge first and last line.
        lines_[start_line] = head + tail;

        // Erase the lines in between (inclusive of end_line).
        auto it_begin = lines_.begin() + start_line + 1;
        auto it_end   = lines_.begin() + end_line + 1;
        lines_.erase(it_begin, it_end);

        NotifyChange(start_line, 1);
    }

    return deleted;
}

// ── Public mutations (with undo recording) ────────────────────────────────

void TextBuffer::InsertText(int line, int column, const std::string& text) {
    if (text.empty()) return;

    RawInsert(line, column, text);

    EditAction action = InsertAction{line, column, text};
    if (in_undo_group_) {
        current_group_.actions.push_back(std::move(action));
    } else {
        undo_stack_.push_back(UndoGroup{{std::move(action)}});
    }
    redo_stack_.clear();
    dirty_ = true;
}

void TextBuffer::DeleteRange(int start_line, int start_col,
                              int end_line,   int end_col) {
    std::string deleted = RawDelete(start_line, start_col, end_line, end_col);
    if (deleted.empty()) return;

    EditAction action = DeleteAction{start_line, start_col, end_line, end_col, deleted};
    if (in_undo_group_) {
        current_group_.actions.push_back(std::move(action));
    } else {
        undo_stack_.push_back(UndoGroup{{std::move(action)}});
    }
    redo_stack_.clear();
    dirty_ = true;
}

void TextBuffer::SetText(const std::string& text) {
    lines_.clear();
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines_.push_back(std::move(line));
    }
    if (lines_.empty()) lines_.emplace_back();
    undo_stack_.clear();
    redo_stack_.clear();
    dirty_ = false;
    NotifyChange(0, GetLineCount());
}

// ── Undo / Redo ──────────────────────────────────────────────────────────

bool TextBuffer::CanUndo() const { return !undo_stack_.empty(); }
bool TextBuffer::CanRedo() const { return !redo_stack_.empty(); }

/// Undo the most recent group of edits by replaying them in reverse.
/// Insert actions are reversed by deleting, and vice-versa.
void TextBuffer::Undo() {
    if (!CanUndo()) return;

    UndoGroup group = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    UndoGroup redo_group;

    // Replay actions in reverse order.
    for (auto it = group.actions.rbegin(); it != group.actions.rend(); ++it) {
        if (auto* ins = std::get_if<InsertAction>(&*it)) {
            // Undo an insert → delete the same text.
            // Calculate the end position of the inserted text.
            int end_line = ins->line;
            int end_col  = ins->column;
            for (char c : ins->text) {
                if (c == '\n') { ++end_line; end_col = 0; }
                else           { ++end_col; }
            }
            std::string deleted = RawDelete(ins->line, ins->column, end_line, end_col);
            redo_group.actions.push_back(InsertAction{ins->line, ins->column, ins->text});
        } else if (auto* del = std::get_if<DeleteAction>(&*it)) {
            // Undo a delete → re-insert the text.
            RawInsert(del->start_line, del->start_col, del->deleted_text);
            redo_group.actions.push_back(DeleteAction{
                del->start_line, del->start_col,
                del->end_line, del->end_col, del->deleted_text});
        }
    }

    redo_stack_.push_back(std::move(redo_group));
    dirty_ = true;
}

/// Redo the most recently undone group by re-applying the original actions.
void TextBuffer::Redo() {
    if (!CanRedo()) return;

    UndoGroup group = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    UndoGroup undo_group;

    for (auto& action : group.actions) {
        if (auto* ins = std::get_if<InsertAction>(&action)) {
            RawInsert(ins->line, ins->column, ins->text);
            undo_group.actions.push_back(*ins);
        } else if (auto* del = std::get_if<DeleteAction>(&action)) {
            RawDelete(del->start_line, del->start_col, del->end_line, del->end_col);
            undo_group.actions.push_back(*del);
        }
    }

    undo_stack_.push_back(std::move(undo_group));
    dirty_ = true;
}

void TextBuffer::BeginUndoGroup() {
    in_undo_group_ = true;
    current_group_.actions.clear();
}

void TextBuffer::EndUndoGroup() {
    in_undo_group_ = false;
    if (!current_group_.actions.empty()) {
        undo_stack_.push_back(std::move(current_group_));
        current_group_.actions.clear();
    }
}

// ── File I/O ─────────────────────────────────────────────────────────────

bool TextBuffer::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    // Strip carriage returns (normalise to LF only).
    std::erase(content, '\r');
    SetText(content);
    dirty_ = false;
    return true;
}

bool TextBuffer::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    for (int i = 0; i < GetLineCount(); ++i) {
        if (i > 0) file << '\n';
        file << lines_[i];
    }
    return file.good();
}

// ── Internal ──────────────────────────────────────────────────────────────

void TextBuffer::NotifyChange(int line, int count) {
    if (on_change_) on_change_(line, count);
}

}  // namespace luce
