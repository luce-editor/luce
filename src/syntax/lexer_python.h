#pragma once
#include "lexer.h"

namespace luce {

/// Hand-written deterministic lexer for Python source files (.py, .pyw, .pyi).
/// Handles keywords, built-in types, decorators, f-strings, triple-quoted docstrings,
/// numbers, comments, and function calls.
class LexerPython : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line, const LexerState& state_in,
                            std::vector<Token>& tokens_out) const override;
    const char* GetLanguageName() const override { return "Python"; }
    std::vector<std::string> GetExtensions() const override {
        return {".py", ".pyw", ".pyi"};
    }
};

}  // namespace luce
