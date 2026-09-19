// ============================================================================
// LexerJson — Implementation of JSON / JSONC tokeniser.
// ============================================================================

#include "lexer_json.h"
#include <cctype>

namespace luce {

LexerState LexerJson::TokenizeLine(std::string_view line, const LexerState& state_in,
                                   std::vector<Token>& tokens_out) const {
    int i = 0;
    int len = static_cast<int>(line.size());
    int state = state_in.state;

    // State 1 = inside block comment /* ... */
    if (state == 1) {
        int start = 0;
        size_t end_idx = line.find("*/");
        if (end_idx != std::string_view::npos) {
            tokens_out.push_back({TokenType::Comment, start, static_cast<int>(end_idx + 2)});
            i = static_cast<int>(end_idx + 2);
            state = 0;
        } else {
            tokens_out.push_back({TokenType::Comment, start, len});
            return {1};
        }
    }

    while (i < len) {
        char c = line[i];

        // 1. Whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }

        // 2. Comments (JSONC support)
        if (c == '/' && i + 1 < len) {
            if (line[i + 1] == '/') {
                tokens_out.push_back({TokenType::Comment, i, len - i});
                break;
            }
            if (line[i + 1] == '*') {
                int start = i;
                size_t end_idx = line.find("*/", i + 2);
                if (end_idx != std::string_view::npos) {
                    int tok_len = static_cast<int>(end_idx + 2) - start;
                    tokens_out.push_back({TokenType::Comment, start, tok_len});
                    i = static_cast<int>(end_idx + 2);
                    continue;
                } else {
                    tokens_out.push_back({TokenType::Comment, start, len - start});
                    return {1};
                }
            }
        }

        // 3. Strings ("...")
        if (c == '"') {
            int start = i++;
            bool escaped = false;
            while (i < len) {
                if (escaped) {
                    escaped = false;
                } else if (line[i] == '\\') {
                    escaped = true;
                } else if (line[i] == '"') {
                    ++i;
                    break;
                }
                ++i;
            }

            // Check if this string is a JSON key (followed by optional whitespace and ':')
            int j = i;
            while (j < len && (line[j] == ' ' || line[j] == '\t')) ++j;
            bool is_key = (j < len && line[j] == ':');

            TokenType tt = is_key ? TokenType::Property : TokenType::String;
            tokens_out.push_back({tt, start, i - start});
            continue;
        }

        // 4. Numbers
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '-' && i + 1 < len && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            int start = i;
            if (line[i] == '-') ++i;
            while (i < len && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
            if (i < len && line[i] == '.') {
                ++i;
                while (i < len && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
            }
            if (i < len && (line[i] == 'e' || line[i] == 'E')) {
                ++i;
                if (i < len && (line[i] == '+' || line[i] == '-')) ++i;
                while (i < len && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
            }
            tokens_out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // 5. Keywords: true, false, null
        if (std::isalpha(static_cast<unsigned char>(c))) {
            int start = i;
            while (i < len && std::isalpha(static_cast<unsigned char>(line[i]))) ++i;
            std::string_view word = line.substr(start, i - start);
            if (word == "true" || word == "false" || word == "null") {
                tokens_out.push_back({TokenType::Keyword, start, i - start});
            } else {
                tokens_out.push_back({TokenType::Identifier, start, i - start});
            }
            continue;
        }

        // 6. Delimiters and operators
        if (c == '{' || c == '}' || c == '[' || c == ']' || c == ',') {
            tokens_out.push_back({TokenType::Punctuation, i, 1});
            ++i;
            continue;
        }

        if (c == ':') {
            tokens_out.push_back({TokenType::Operator, i, 1});
            ++i;
            continue;
        }

        // 7. Fallback
        tokens_out.push_back({TokenType::None, i, 1});
        ++i;
    }

    return {0};
}

}  // namespace luce
