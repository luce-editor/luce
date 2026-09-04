#pragma once
// ============================================================================
// SyntaxHighlighter — Manages lexers, caches tokenised lines, and maps
// file extensions to the appropriate language lexer.
// ============================================================================

#include "lexer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace luce {

class SyntaxHighlighter {
public:
    SyntaxHighlighter();

    /// Select the active lexer based on a file extension (e.g. ".cpp").
    /// Returns true if a matching lexer was found.
    bool SetLanguageByExtension(const std::string& extension);

    /// Explicitly set the active lexer by language name (e.g. "C/C++").
    bool SetLanguageByName(const std::string& name);

    /// Return the human-readable language name, or "Plain Text" if none.
    const char* GetLanguageName() const;

    /// Tokenise a single line using the active lexer.
    /// Manages per-line state so that multi-line constructs work.
    const std::vector<Token>& GetTokensForLine(int line_index,
                                                const std::string& line_text);

    /// Invalidate cached tokens for lines in range [first, first+count).
    /// Called by TextBuffer's change callback.
    void InvalidateLines(int first, int count);

    /// Invalidate the entire cache (e.g. when switching language).
    void InvalidateAll();

    /// Return a list of all registered language names.
    std::vector<std::string> GetLanguageNames() const;

private:
    struct LineCacheEntry {
        std::string          text;    // Snapshot of the line when tokenised.
        LexerState           state;   // State after tokenising this line.
        std::vector<Token>   tokens;
        bool                 valid = false;
    };

    /// Re-tokenise from `from_line` downward until the cached state matches.
    void RetokenizeFrom(int from_line, const std::string& line_text);

    std::vector<std::unique_ptr<Lexer>>                  lexers_;
    Lexer*                                               active_lexer_ = nullptr;
    std::unordered_map<std::string, Lexer*>              ext_map_;   // ".cpp" → Lexer*
    std::unordered_map<std::string, Lexer*>              name_map_;  // "C/C++" → Lexer*
    std::vector<LineCacheEntry>                           cache_;
};

}  // namespace luce
