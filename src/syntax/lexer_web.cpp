// ============================================================================
// LexerWeb — Tokeniser for HTML / CSS / JavaScript.
//
// State encoding (stored in LexerState::state):
//   0     = HTML normal
//   1     = HTML inside a tag (after '<' before '>')
//   2     = inside <!-- comment -->
//   10-19 = CSS context  (10 = normal, 11 = block comment, 12 = string)
//   20-29 = JS context   (20 = normal, 21 = block comment, 22 = string,
//                          23 = template literal)
// ============================================================================

#include "lexer_web.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace luce {

namespace {

// ── JavaScript keywords ───────────────────────────────────────────────────
const std::unordered_set<std::string> kJSKeywords = {
    "async", "await", "break", "case", "catch", "class", "const",
    "continue", "debugger", "default", "delete", "do", "else", "export",
    "extends", "finally", "for", "from", "function", "if", "import", "in",
    "instanceof", "let", "new", "of", "return", "static", "super",
    "switch", "this", "throw", "try", "typeof", "var", "void", "while",
    "with", "yield", "true", "false", "null", "undefined", "NaN",
    "Infinity", "console", "document", "window",
};

// ── CSS properties (common subset) ────────────────────────────────────────
const std::unordered_set<std::string> kCSSProperties = {
    "color", "background", "background-color", "margin", "padding", "border",
    "width", "height", "display", "position", "top", "left", "right", "bottom",
    "font-size", "font-family", "font-weight", "text-align", "flex",
    "grid", "gap", "align-items", "justify-content", "z-index", "opacity",
    "transition", "transform", "animation", "overflow", "box-shadow",
    "border-radius", "cursor", "visibility", "content",
};

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$'; }
bool IsIdentChar(char c)  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$' || c == '-'; }
bool IsDigit(char c)      { return std::isdigit(static_cast<unsigned char>(c)); }

/// Case-insensitive check for a substring at position `i` in `line`.
bool MatchInsensitive(std::string_view line, int i, std::string_view target) {
    if (i + static_cast<int>(target.size()) > static_cast<int>(line.size())) return false;
    for (size_t k = 0; k < target.size(); ++k) {
        if (std::tolower(static_cast<unsigned char>(line[i + k])) !=
            std::tolower(static_cast<unsigned char>(target[k])))
            return false;
    }
    return true;
}

}  // namespace

// ── Main entry point ──────────────────────────────────────────────────────

LexerState LexerWeb::TokenizeLine(std::string_view line,
                                   const LexerState& state_in,
                                   std::vector<Token>& out) const {
    out.clear();
    int state = state_in.state;
    int i     = 0;
    int len   = static_cast<int>(line.size());

    while (i < len) {
        if (state >= 20) {
            // JavaScript context
            int prev = state;
            state = TokenizeJS(line, i, state, out);
            // Advance past what was consumed.
            if (!out.empty()) i = out.back().start + out.back().length;
            else ++i;
            // Check for </script>
            if (state < 20 && state >= 0) continue;
        } else if (state >= 10) {
            // CSS context
            state = TokenizeCSS(line, i, state, out);
            if (!out.empty()) i = out.back().start + out.back().length;
            else ++i;
            if (state < 10 && state >= 0) continue;
        } else {
            // HTML context
            state = TokenizeHTML(line, i, state, out);
            if (!out.empty()) i = out.back().start + out.back().length;
            else ++i;
        }
    }

    return {state};
}

// ── HTML tokeniser ────────────────────────────────────────────────────────

int LexerWeb::TokenizeHTML(std::string_view line, int i, int state,
                            std::vector<Token>& out) const {
    int len = static_cast<int>(line.size());

    // Continuing HTML comment.
    if (state == 2) {
        int start = i;
        while (i + 2 < len) {
            if (line[i] == '-' && line[i + 1] == '-' && line[i + 2] == '>') {
                i += 3;
                out.push_back({TokenType::Comment, start, i - start});
                return 0;
            }
            ++i;
        }
        i = len;
        out.push_back({TokenType::Comment, start, i - start});
        return 2;
    }

    // Inside a tag (attributes).
    if (state == 1) {
        while (i < len) {
            char c = line[i];
            if (c == '>') {
                out.push_back({TokenType::TagBracket, i, 1});
                ++i;
                return 0;
            }
            if (c == '"' || c == '\'') {
                int start = i++;
                while (i < len && line[i] != c) ++i;
                if (i < len) ++i;
                out.push_back({TokenType::Value, start, i - start});
                continue;
            }
            if (c == '=' ) { out.push_back({TokenType::Operator, i, 1}); ++i; continue; }
            if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
            // Attribute name.
            int start = i;
            while (i < len && !std::isspace(static_cast<unsigned char>(line[i])) &&
                   line[i] != '=' && line[i] != '>') ++i;
            out.push_back({TokenType::Property, start, i - start});
        }
        return 1;
    }

    // Normal HTML.
    while (i < len) {
        // HTML comment start.
        if (i + 3 < len && line[i] == '<' && line[i+1] == '!' && line[i+2] == '-' && line[i+3] == '-') {
            int start = i;
            i += 4;
            while (i + 2 < len) {
                if (line[i] == '-' && line[i+1] == '-' && line[i+2] == '>') {
                    i += 3;
                    out.push_back({TokenType::Comment, start, i - start});
                    return 0;
                }
                ++i;
            }
            i = len;
            out.push_back({TokenType::Comment, start, i - start});
            return 2;
        }

        // Tag start.
        if (line[i] == '<') {
            int start = i;
            out.push_back({TokenType::TagBracket, i, 1});
            ++i;
            bool closing = (i < len && line[i] == '/');
            if (closing) { out.push_back({TokenType::TagBracket, i, 1}); ++i; }
            // Tag name.
            int name_start = i;
            while (i < len && IsIdentChar(line[i])) ++i;
            if (i > name_start) {
                std::string tag_name(line.substr(name_start, i - name_start));
                out.push_back({TokenType::Tag, name_start, i - name_start});
                // Switch context for <script> and <style>.
                if (!closing) {
                    std::string lower_tag = tag_name;
                    std::ranges::transform(lower_tag, lower_tag.begin(), ::tolower);
                    if (lower_tag == "script") {
                        // Find '>' to end the tag, then switch to JS.
                        while (i < len && line[i] != '>') {
                            if (line[i] == '"' || line[i] == '\'') {
                                char q = line[i]; int qs = i++;
                                while (i < len && line[i] != q) ++i;
                                if (i < len) ++i;
                                out.push_back({TokenType::Value, qs, i - qs});
                            } else { ++i; }
                        }
                        if (i < len) { out.push_back({TokenType::TagBracket, i, 1}); ++i; }
                        return 20;  // Enter JS context.
                    }
                    if (lower_tag == "style") {
                        while (i < len && line[i] != '>') {
                            if (line[i] == '"' || line[i] == '\'') {
                                char q = line[i]; int qs = i++;
                                while (i < len && line[i] != q) ++i;
                                if (i < len) ++i;
                                out.push_back({TokenType::Value, qs, i - qs});
                            } else { ++i; }
                        }
                        if (i < len) { out.push_back({TokenType::TagBracket, i, 1}); ++i; }
                        return 10;  // Enter CSS context.
                    }
                }
            }
            return 1;  // Inside tag (attributes follow).
        }

        // Plain text — skip.
        ++i;
    }
    return 0;
}

// ── CSS tokeniser ─────────────────────────────────────────────────────────

int LexerWeb::TokenizeCSS(std::string_view line, int i, int state,
                           std::vector<Token>& out) const {
    int len = static_cast<int>(line.size());

    // Check for </style> to leave CSS context.
    for (int j = i; j + 7 < len; ++j) {
        if (MatchInsensitive(line, j, "</style>")) {
            // Tokenize remaining CSS up to j, then emit the closing tag.
            // (simplified: just emit the closing tag)
            if (j > i) out.push_back({TokenType::None, i, j - i});
            out.push_back({TokenType::Tag, j, 8});
            return 0;
        }
    }

    // Block comment continuation.
    if (state == 11) {
        int start = i;
        while (i + 1 < len) {
            if (line[i] == '*' && line[i + 1] == '/') {
                i += 2;
                out.push_back({TokenType::Comment, start, i - start});
                return 10;
            }
            ++i;
        }
        i = len;
        out.push_back({TokenType::Comment, start, i - start});
        return 11;
    }

    while (i < len) {
        char c = line[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }

        // Block comment.
        if (i + 1 < len && c == '/' && line[i + 1] == '*') {
            int start = i; i += 2;
            while (i + 1 < len) {
                if (line[i] == '*' && line[i+1] == '/') { i += 2; out.push_back({TokenType::Comment, start, i - start}); return 10; }
                ++i;
            }
            i = len;
            out.push_back({TokenType::Comment, start, i - start});
            return 11;
        }

        // String.
        if (c == '"' || c == '\'') {
            int start = i++;
            while (i < len && line[i] != c) { if (line[i] == '\\') ++i; ++i; }
            if (i < len) ++i;
            out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // Number.
        if (IsDigit(c) || (c == '.' && i + 1 < len && IsDigit(line[i + 1]))) {
            int start = i;
            while (i < len && (IsDigit(line[i]) || line[i] == '.' || line[i] == '%' ||
                               std::isalpha(static_cast<unsigned char>(line[i])))) ++i;
            out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // @ rules.
        if (c == '@') {
            int start = i++;
            while (i < len && IsIdentChar(line[i])) ++i;
            out.push_back({TokenType::Keyword, start, i - start});
            continue;
        }

        // Selector or property.
        if (IsIdentStart(c) || c == '.' || c == '#') {
            int start = i;
            while (i < len && (IsIdentChar(line[i]) || line[i] == '.' || line[i] == '#')) ++i;
            std::string word(line.substr(start, i - start));
            TokenType tt = kCSSProperties.contains(word) ? TokenType::Property : TokenType::Tag;
            out.push_back({tt, start, i - start});
            continue;
        }

        // Punctuation.
        if (c == '{' || c == '}' || c == ':' || c == ';' || c == ',') {
            out.push_back({TokenType::Punctuation, i, 1});
            ++i; continue;
        }

        out.push_back({TokenType::Operator, i, 1});
        ++i;
    }
    return 10;
}

// ── JavaScript tokeniser ──────────────────────────────────────────────────

int LexerWeb::TokenizeJS(std::string_view line, int i, int state,
                          std::vector<Token>& out) const {
    int len = static_cast<int>(line.size());

    // Check for </script> to leave JS context.
    for (int j = i; j + 8 < len; ++j) {
        if (MatchInsensitive(line, j, "</script>")) {
            if (j > i) out.push_back({TokenType::None, i, j - i});
            out.push_back({TokenType::Tag, j, 9});
            return 0;
        }
    }

    // Block comment continuation.
    if (state == 21) {
        int start = i;
        while (i + 1 < len) {
            if (line[i] == '*' && line[i + 1] == '/') { i += 2; out.push_back({TokenType::Comment, start, i - start}); return 20; }
            ++i;
        }
        i = len;
        out.push_back({TokenType::Comment, start, i - start});
        return 21;
    }

    // Template literal continuation.
    if (state == 23) {
        int start = i;
        while (i < len) {
            if (line[i] == '\\') { i += 2; continue; }
            if (line[i] == '`')  { ++i; out.push_back({TokenType::String, start, i - start}); return 20; }
            ++i;
        }
        out.push_back({TokenType::String, start, i - start});
        return 23;
    }

    while (i < len) {
        char c = line[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }

        // Line comment.
        if (i + 1 < len && c == '/' && line[i + 1] == '/') {
            out.push_back({TokenType::Comment, i, len - i});
            return 20;
        }

        // Block comment.
        if (i + 1 < len && c == '/' && line[i + 1] == '*') {
            int start = i; i += 2;
            while (i + 1 < len) {
                if (line[i] == '*' && line[i+1] == '/') { i += 2; out.push_back({TokenType::Comment, start, i - start}); goto js_continue; }
                ++i;
            }
            i = len;
            out.push_back({TokenType::Comment, start, i - start});
            return 21;
        }

        // Template literal.
        if (c == '`') {
            int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == '`')  { ++i; out.push_back({TokenType::String, start, i - start}); goto js_continue; }
                ++i;
            }
            out.push_back({TokenType::String, start, i - start});
            return 23;
        }

        // String.
        if (c == '"' || c == '\'') {
            int start = i++;
            while (i < len && line[i] != c) { if (line[i] == '\\') ++i; ++i; }
            if (i < len) ++i;
            out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // Number.
        if (IsDigit(c)) {
            int start = i;
            while (i < len && (IsDigit(line[i]) || line[i] == '.' || line[i] == 'x' || line[i] == 'X' ||
                               IsIdentChar(line[i]))) ++i;
            out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // Identifier / keyword.
        if (IsIdentStart(c)) {
            int start = i;
            while (i < len && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_' || line[i] == '$')) ++i;
            std::string word(line.substr(start, i - start));
            TokenType tt = kJSKeywords.contains(word) ? TokenType::Keyword : TokenType::Identifier;
            // Heuristic: function call.
            if (tt == TokenType::Identifier) {
                int j = i;
                while (j < len && line[j] == ' ') ++j;
                if (j < len && line[j] == '(') tt = TokenType::Function;
            }
            out.push_back({tt, start, i - start});
            continue;
        }

        // Punctuation / operators.
        if (c == '{' || c == '}' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == ';' || c == ',' || c == '.') {
            out.push_back({TokenType::Punctuation, i, 1});
        } else {
            out.push_back({TokenType::Operator, i, 1});
        }
        ++i;
        continue;

    js_continue:;
    }
    return 20;
}

}  // namespace luce
