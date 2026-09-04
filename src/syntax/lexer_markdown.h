#pragma once
#include "lexer.h"

namespace luce {

/// Hand-written lexer for Markdown files (.md, .markdown).
/// Tokens: Headings (#), Bold (**), Italic (*), Code (`...` and ```...```),
/// Lists (- * 1.), Links/Images ([...](...), ![...](...)), Blockquotes (>).
class LexerMarkdown : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line, const LexerState& state_in,
                             std::vector<Token>& tokens_out) const override;
    const char* GetLanguageName() const override { return "Markdown"; }
    std::vector<std::string> GetExtensions() const override {
        return {".md", ".markdown", ".mdown", ".mkdn"};
    }
};

}  // namespace luce
