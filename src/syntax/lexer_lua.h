#pragma once
// ============================================================================
// LexerLua — Hand-written deterministic lexer for Lua source files (.lua).
//
// Supports Lua 5.4 syntax:
// - Keywords (local, function, end, if, then, else, elseif, for, while, etc.)
// - Built-in types and standard library (string, table, math, io, os, etc.)
// - Luce Editor API (luce.* functions, luce.plugin table, lifecycle hooks)
// - Single-line comments (--) and multi-line block comments (--[[ ... ]])
// - Strings ('...', "...", and multi-line [[ ... ]])
// - Numbers (decimal, float, exponent, hexadecimal 0x...)
// - Operators (+, -, *, /, //, %, ^, #, ==, ~=, <=, >=, .., etc.)
// ============================================================================

#include "lexer.h"

namespace luce {

class LexerLua : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line, const LexerState& state_in,
                            std::vector<Token>& tokens_out) const override;

    const char* GetLanguageName() const override { return "Lua"; }

    std::vector<std::string> GetExtensions() const override {
        return {".lua"};
    }
};

}  // namespace luce
