#pragma once
// ============================================================================
// LexerCMake — Tokeniser for CMakeLists.txt and *.cmake files.
// ============================================================================

#include "lexer.h"

namespace luce {

class LexerCMake : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line,
                            const LexerState& state_in,
                            std::vector<Token>& tokens_out) const override;

    const char* GetLanguageName() const override { return "CMake"; }
    std::vector<std::string> GetExtensions() const override {
        return {".cmake", "cmakelists.txt"};
    }
};

}  // namespace luce
