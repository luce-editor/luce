#pragma once
#include "lexer.h"

namespace luce {

/// Lexer for web files: HTML, CSS (inside <style>), and JavaScript
/// (inside <script>).  Handles context switching between the three
/// sub-languages within a single document.
class LexerWeb : public Lexer {
public:
    LexerState TokenizeLine(std::string_view line, const LexerState& state_in,
                             std::vector<Token>& tokens_out) const override;
    const char* GetLanguageName() const override { return "HTML"; }
    std::vector<std::string> GetExtensions() const override {
        return {".html", ".htm", ".css", ".js", ".jsx", ".mjs"};
    }

private:
    // Sub-language tokenisers called by the main dispatch.
    int TokenizeHTML(std::string_view line, int i, int state,
                     std::vector<Token>& out) const;
    int TokenizeCSS(std::string_view line, int i, int state,
                    std::vector<Token>& out) const;
    int TokenizeJS(std::string_view line, int i, int state,
                   std::vector<Token>& out) const;
};

}  // namespace luce
