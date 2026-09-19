#pragma once
// ============================================================================
// LexerJson — Tokeniser for JSON and JSONC (JSON with Comments).
// ============================================================================

#include "lexer.h"

namespace luce {

/// Fast hand-written state-machine lexer for JSON / JSONC.
/// Distinguishes JSON keys from value strings, tokenizes booleans (true, false),
/// null, numbers, comments (// and /* */), punctuation ({}, []), and colons.
class LexerJson : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line, const LexerState& state_in,
                            std::vector<Token>& tokens_out) const override;

    const char* GetLanguageName() const override { return "JSON"; }

    std::vector<std::string> GetExtensions() const override {
        return {".json", ".jsonc", ".sublime-settings", ".code-workspace"};
    }
};

}  // namespace luce
