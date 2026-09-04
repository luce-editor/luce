#pragma once
#include "lexer.h"

namespace luce {

/// Hand-written lexer for C and C++ source files.
/// Covers keywords (C11/C++23), preprocessor directives, string/char
/// literals (including raw strings), comments, numbers, and operators.
class LexerCpp : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line, const LexerState& state_in,
                             std::vector<Token>& tokens_out) const override;
    const char* GetLanguageName() const override { return "C/C++"; }
    std::vector<std::string> GetExtensions() const override {
        return {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".hxx", ".inl"};
    }
};

}  // namespace luce
