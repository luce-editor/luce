#pragma once
#include "lexer.h"

namespace luce {

/// Hand-written lexer for Rust source files.
/// Covers keywords, types, lifetime annotations, macro invocations,
/// attributes, raw/byte strings, and Rust-specific number formats.
class LexerRust : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line, const LexerState& state_in,
                             std::vector<Token>& tokens_out) const override;
    const char* GetLanguageName() const override { return "Rust"; }
    std::vector<std::string> GetExtensions() const override {
        return {".rs"};
    }
};

}  // namespace luce
