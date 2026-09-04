#include "lexer_python.h"

#include <unordered_set>
#include <string>

namespace luce {

namespace {

const std::unordered_set<std::string_view> kKeywords = {
    "and", "as", "assert", "async", "await", "break", "class", "continue",
    "def", "del", "elif", "else", "except", "finally", "for", "from",
    "global", "if", "import", "in", "is", "lambda", "nonlocal", "not",
    "or", "pass", "raise", "return", "try", "while", "with", "yield",
    "match", "case", "type"
};

const std::unordered_set<std::string_view> kBuiltins = {
    "True", "False", "None", "self", "cls", "int", "float", "str", "bool",
    "list", "dict", "set", "tuple", "bytes", "bytearray", "range",
    "enumerate", "zip", "map", "filter", "print", "len", "open", "input",
    "super", "isinstance", "issubclass", "hasattr", "getattr", "setattr",
    "id", "type", "any", "all", "min", "max", "sum", "abs", "round",
    "object", "Exception", "BaseException", "ValueError", "TypeError",
    "KeyError", "IndexError", "FileNotFoundError", "RuntimeError", "StopIteration"
};

bool IsIdentStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool IsIdentChar(char c) {
    return IsIdentStart(c) || (c >= '0' && c <= '9');
}

}  // namespace

LexerState LexerPython::TokenizeLine(std::string_view line, const LexerState& state_in,
                                     std::vector<Token>& tokens_out) const {
    LexerState state_out = state_in;
    int i = 0;
    int len = static_cast<int>(line.length());

    // State 1: Inside triple double-quoted string """
    // State 2: Inside triple single-quoted string '''
    if (state_out.state == 1 || state_out.state == 2) {
        std::string_view close_delim = (state_out.state == 1) ? "\"\"\"" : "'''";
        size_t close_pos = line.find(close_delim);
        if (close_pos != std::string_view::npos) {
            int end_idx = static_cast<int>(close_pos) + 3;
            tokens_out.push_back({TokenType::String, 0, end_idx});
            i = end_idx;
            state_out.state = 0;
        } else {
            tokens_out.push_back({TokenType::String, 0, len});
            return state_out;
        }
    }

    while (i < len) {
        char c = line[i];

        // Whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }

        // Single-line comment
        if (c == '#') {
            tokens_out.push_back({TokenType::Comment, i, len - i});
            break;
        }

        // Decorator (@classmethod, @property, etc.)
        if (c == '@') {
            int start = i++;
            while (i < len && (IsIdentChar(line[i]) || line[i] == '.')) {
                ++i;
            }
            tokens_out.push_back({TokenType::Attribute, start, i - start});
            continue;
        }

        // Check for string prefixes (r, u, f, b, fr, rf, br, rb) before quotes
        int prefix_start = i;
        int check_idx = i;
        if ((line[check_idx] == 'r' || line[check_idx] == 'R' ||
             line[check_idx] == 'u' || line[check_idx] == 'U' ||
             line[check_idx] == 'f' || line[check_idx] == 'F' ||
             line[check_idx] == 'b' || line[check_idx] == 'B') &&
            check_idx + 1 < len) {
            check_idx++;
            if ((line[check_idx] == 'r' || line[check_idx] == 'R' ||
                 line[check_idx] == 'f' || line[check_idx] == 'F' ||
                 line[check_idx] == 'b' || line[check_idx] == 'B') &&
                check_idx + 1 < len) {
                check_idx++;
            }
        }

        // Triple double quotes """
        if (check_idx + 2 < len && line[check_idx] == '"' && line[check_idx + 1] == '"' && line[check_idx + 2] == '"') {
            int start = prefix_start;
            i = check_idx + 3;
            size_t close_pos = line.substr(i).find("\"\"\"");
            if (close_pos != std::string_view::npos) {
                i += static_cast<int>(close_pos) + 3;
                tokens_out.push_back({TokenType::String, start, i - start});
            } else {
                tokens_out.push_back({TokenType::String, start, len - start});
                state_out.state = 1;
                return state_out;
            }
            continue;
        }

        // Triple single quotes '''
        if (check_idx + 2 < len && line[check_idx] == '\'' && line[check_idx + 1] == '\'' && line[check_idx + 2] == '\'') {
            int start = prefix_start;
            i = check_idx + 3;
            size_t close_pos = line.substr(i).find("'''");
            if (close_pos != std::string_view::npos) {
                i += static_cast<int>(close_pos) + 3;
                tokens_out.push_back({TokenType::String, start, i - start});
            } else {
                tokens_out.push_back({TokenType::String, start, len - start});
                state_out.state = 2;
                return state_out;
            }
            continue;
        }

        // Regular single / double quoted string (with prefix if any)
        if (line[check_idx] == '"' || line[check_idx] == '\'') {
            char quote = line[check_idx];
            int start = prefix_start;
            i = check_idx + 1;
            bool escaped = false;
            while (i < len) {
                if (escaped) {
                    escaped = false;
                } else if (line[i] == '\\') {
                    escaped = true;
                } else if (line[i] == quote) {
                    ++i;
                    break;
                }
                ++i;
            }
            tokens_out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // Numbers (0x..., 0b..., 0o..., decimal, floats)
        if ((c >= '0' && c <= '9') || (c == '.' && i + 1 < len && line[i + 1] >= '0' && line[i + 1] <= '9')) {
            int start = i++;
            if (c == '0' && i < len && (line[i] == 'x' || line[i] == 'X' ||
                                        line[i] == 'b' || line[i] == 'B' ||
                                        line[i] == 'o' || line[i] == 'O')) {
                ++i;
                while (i < len && (isxdigit(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                    ++i;
                }
            } else {
                bool has_dot = (c == '.');
                while (i < len) {
                    if (line[i] >= '0' && line[i] <= '9') {
                        ++i;
                    } else if (line[i] == '.' && !has_dot) {
                        has_dot = true;
                        ++i;
                    } else if (line[i] == '_' || line[i] == 'e' || line[i] == 'E' || line[i] == 'j' || line[i] == 'J') {
                        if ((line[i] == 'e' || line[i] == 'E') && i + 1 < len && (line[i + 1] == '+' || line[i + 1] == '-')) {
                            i += 2;
                        } else {
                            ++i;
                        }
                    } else {
                        break;
                    }
                }
            }
            tokens_out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // Identifiers and keywords
        if (IsIdentStart(c)) {
            int start = i++;
            while (i < len && IsIdentChar(line[i])) {
                ++i;
            }
            std::string_view word = line.substr(start, i - start);

            if (kKeywords.contains(word)) {
                tokens_out.push_back({TokenType::Keyword, start, i - start});
            } else if (kBuiltins.contains(word)) {
                tokens_out.push_back({TokenType::Type, start, i - start});
            } else {
                // Check if followed by '(' -> Function call
                int peek = i;
                while (peek < len && (line[peek] == ' ' || line[peek] == '\t')) {
                    ++peek;
                }
                if (peek < len && line[peek] == '(') {
                    tokens_out.push_back({TokenType::Function, start, i - start});
                } else {
                    tokens_out.push_back({TokenType::Identifier, start, i - start});
                }
            }
            continue;
        }

        // Operators & Punctuation
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == ',' || c == ':' || c == ';' || c == '.') {
            tokens_out.push_back({TokenType::Punctuation, i, 1});
            ++i;
            continue;
        }

        // Multi-char operators
        int start = i++;
        while (i < len && (line[i] == '=' || line[i] == '+' || line[i] == '-' ||
                           line[i] == '*' || line[i] == '/' || line[i] == '%' ||
                           line[i] == '<' || line[i] == '>' || line[i] == '!' ||
                           line[i] == '&' || line[i] == '|' || line[i] == '^' ||
                           line[i] == '~')) {
            ++i;
        }
        tokens_out.push_back({TokenType::Operator, start, i - start});
    }

    return state_out;
}

}  // namespace luce
