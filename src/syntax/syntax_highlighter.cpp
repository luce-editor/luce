// ============================================================================
// SyntaxHighlighter — Implementation.
// ============================================================================

#include "syntax_highlighter.h"
#include "lexer_cmake.h"
#include "lexer_cpp.h"
#include "lexer_markdown.h"
#include "lexer_python.h"
#include "lexer_rust.h"
#include "lexer_web.h"

#include <algorithm>

namespace luce {

SyntaxHighlighter::SyntaxHighlighter() {
    // Register built-in lexers.  Each lexer declares its own extensions.
    auto register_lexer = [&](std::unique_ptr<Lexer> lexer) {
        Lexer* raw = lexer.get();
        name_map_[raw->GetLanguageName()] = raw;
        for (auto& ext : raw->GetExtensions()) {
            ext_map_[ext] = raw;
        }
        lexers_.push_back(std::move(lexer));
    };

    register_lexer(std::make_unique<LexerCpp>());
    register_lexer(std::make_unique<LexerWeb>());
    register_lexer(std::make_unique<LexerRust>());
    register_lexer(std::make_unique<LexerPython>());
    register_lexer(std::make_unique<LexerMarkdown>());
    register_lexer(std::make_unique<LexerCMake>());
}

bool SyntaxHighlighter::SetLanguageByExtension(const std::string& ext) {
    std::string lower_ext = ext;
    std::ranges::transform(lower_ext, lower_ext.begin(), ::tolower);

    // First try exact extension or full name match
    auto it = ext_map_.find(lower_ext);
    if (it != ext_map_.end()) {
        active_lexer_ = it->second;
        InvalidateAll();
        return true;
    }
    
    // Check if filename is CMakeLists.txt
    if (ext.find("CMakeLists.txt") != std::string::npos || lower_ext == "cmakelists.txt") {
        auto it_cm = ext_map_.find(".cmake");
        if (it_cm != ext_map_.end()) {
            active_lexer_ = it_cm->second;
            InvalidateAll();
            return true;
        }
    }
    active_lexer_ = nullptr;
    InvalidateAll();
    return false;
}

bool SyntaxHighlighter::SetLanguageByName(const std::string& name) {
    auto it = name_map_.find(name);
    if (it != name_map_.end()) {
        active_lexer_ = it->second;
        InvalidateAll();
        return true;
    }
    return false;
}

const char* SyntaxHighlighter::GetLanguageName() const {
    return active_lexer_ ? active_lexer_->GetLanguageName() : "Plain Text";
}

/// Return cached tokens for a line, re-tokenising if the cache is stale.
/// This is called once per visible line per frame, so it must be fast
/// in the common case (cache hit).
const std::vector<Token>& SyntaxHighlighter::GetTokensForLine(
        int line_index, const std::string& line_text) {
    static const std::vector<Token> kEmpty;
    if (!active_lexer_) return kEmpty;

    // Grow cache if needed.
    if (line_index >= static_cast<int>(cache_.size())) {
        cache_.resize(line_index + 256);
    }

    auto& entry = cache_[line_index];
    if (entry.valid && entry.text == line_text) {
        return entry.tokens;
    }

    // Determine the incoming state from the previous line's cache.
    LexerState prev_state{};
    if (line_index > 0 && line_index - 1 < static_cast<int>(cache_.size()) &&
        cache_[line_index - 1].valid) {
        prev_state = cache_[line_index - 1].state;
    }

    entry.tokens.clear();
    entry.state = active_lexer_->TokenizeLine(line_text, prev_state, entry.tokens);
    entry.text  = line_text;
    entry.valid = true;
    return entry.tokens;
}

void SyntaxHighlighter::InvalidateLines(int first, int count) {
    // Invalidate affected lines and all subsequent lines (because multi-line
    // state may have changed).
    for (int i = first; i < static_cast<int>(cache_.size()); ++i) {
        cache_[i].valid = false;
    }
}

void SyntaxHighlighter::InvalidateAll() {
    cache_.clear();
}

std::vector<std::string> SyntaxHighlighter::GetLanguageNames() const {
    std::vector<std::string> names;
    for (auto& [name, _] : name_map_) {
        names.push_back(name);
    }
    std::ranges::sort(names);
    return names;
}

}  // namespace luce
