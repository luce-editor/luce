// ============================================================================
// LexerMarkdown — Hand-written tokeniser for Markdown files.
//
// State encoding:
//   0 = normal
//   1 = inside code block (``` ... ```)
// ============================================================================

#include "lexer_markdown.h"
#include <cctype>

namespace luce {

LexerState LexerMarkdown::TokenizeLine(std::string_view line,
                                       const LexerState& state_in,
                                       std::vector<Token>& out) const {
    out.clear();
    int len = static_cast<int>(line.size());
    int i = 0;
    int state = state_in.state;

    // Check for code fence start/end
    auto is_code_fence = [&]() {
        int idx = 0;
        while (idx < len && line[idx] == ' ') ++idx;
        return (idx + 2 < len && line[idx] == '`' && line[idx+1] == '`' && line[idx+2] == '`');
    };

    if (state == 1) {
        if (is_code_fence()) {
            out.push_back({TokenType::Keyword, 0, len});
            return {0}; // Code block ended
        } else {
            out.push_back({TokenType::String, 0, len});
            return {1}; // Still inside code block
        }
    }

    if (is_code_fence()) {
        out.push_back({TokenType::Keyword, 0, len});
        return {1}; // Code block started
    }

    // Skip leading spaces for prefix checks
    int first_non_ws = 0;
    while (first_non_ws < len && line[first_non_ws] == ' ') ++first_non_ws;

    if (first_non_ws < len) {
        // Headings (#, ##, ###, ...)
        if (line[first_non_ws] == '#') {
            out.push_back({TokenType::Keyword, 0, len});
            return {0};
        }
        // Blockquotes (>)
        if (line[first_non_ws] == '>') {
            out.push_back({TokenType::Comment, 0, len});
            return {0};
        }
        // Horizontal rule (---, ***, ___)
        if ((line[first_non_ws] == '-' || line[first_non_ws] == '*' || line[first_non_ws] == '_') &&
            first_non_ws + 2 < len &&
            line[first_non_ws+1] == line[first_non_ws] &&
            line[first_non_ws+2] == line[first_non_ws]) {
            out.push_back({TokenType::Punctuation, 0, len});
            return {0};
        }
    }

    // Inline parsing
    while (i < len) {
        char c = line[i];

        // Inline code (`...`)
        if (c == '`') {
            int start = i++;
            while (i < len && line[i] != '`') ++i;
            if (i < len) ++i;
            out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // Links [text](url) or Images ![alt](url)
        if (c == '[' || (c == '!' && i + 1 < len && line[i+1] == '[')) {
            int start = i;
            if (c == '!') ++i;
            int bracket_start = i++;
            while (i < len && line[i] != ']') ++i;
            if (i < len && line[i] == ']') {
                ++i;
                if (i < len && line[i] == '(') {
                    while (i < len && line[i] != ')') ++i;
                    if (i < len) ++i;
                    out.push_back({TokenType::Function, start, i - start});
                    continue;
                }
            }
            i = start + 1;
            continue;
        }

        // Bold (**text** or __text__)
        if ((c == '*' && i + 1 < len && line[i+1] == '*') ||
            (c == '_' && i + 1 < len && line[i+1] == '_')) {
            char mark = c;
            int start = i;
            i += 2;
            while (i + 1 < len && !(line[i] == mark && line[i+1] == mark)) ++i;
            if (i + 1 < len) i += 2;
            out.push_back({TokenType::Type, start, i - start});
            continue;
        }

        // Italic (*text* or _text_)
        if ((c == '*' || c == '_') && i + 1 < len && !std::isspace(static_cast<unsigned char>(line[i+1]))) {
            char mark = c;
            int start = i++;
            while (i < len && line[i] != mark) ++i;
            if (i < len) ++i;
            out.push_back({TokenType::Attribute, start, i - start});
            continue;
        }

        // List item marker (- , * , + , 1. )
        if (i == first_non_ws && (c == '-' || c == '*' || c == '+') && i + 1 < len && line[i+1] == ' ') {
            out.push_back({TokenType::Operator, i, 2});
            i += 2;
            continue;
        }
        if (i == first_non_ws && std::isdigit(static_cast<unsigned char>(c))) {
            int start = i;
            while (i < len && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
            if (i < len && (line[i] == '.' || line[i] == ')') && i + 1 < len && line[i+1] == ' ') {
                i += 2;
                out.push_back({TokenType::Operator, start, i - start});
                continue;
            }
            i = start;
        }

        ++i;
    }

    return {0};
}

}  // namespace luce
