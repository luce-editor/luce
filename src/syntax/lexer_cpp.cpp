// ============================================================================
// LexerCpp — Hand-written tokeniser for C / C++ source files.
//
// Lexer states:
//   0 = normal
//   1 = inside a block comment (/* ... */)
//   2 = inside a raw string literal (R"delim(...)delim")
// ============================================================================

#include "lexer_cpp.h"
#include <algorithm>
#include <unordered_set>

namespace luce {

namespace {

const std::unordered_set<std::string> kKeywords = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
    "break", "case", "catch", "class", "compl", "concept", "const",
    "consteval", "constexpr", "constinit", "const_cast", "continue",
    "co_await", "co_return", "co_yield", "decltype", "default", "delete",
    "do", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
    "for", "friend", "goto", "if", "inline", "mutable", "namespace", "new",
    "noexcept", "not", "not_eq", "operator", "or", "or_eq", "private",
    "protected", "public", "register", "reinterpret_cast", "requires",
    "return", "sizeof", "static", "static_assert", "static_cast", "struct",
    "switch", "template", "this", "throw", "try", "typedef", "typeid",
    "typename", "union", "using", "virtual", "volatile", "while", "xor",
    "xor_eq", "override", "final", "import", "module", "nullptr",
    "true", "false", "atomic_cancel", "atomic_commit", "atomic_noexcept",
    "contract_assert", "reflexpr", "synchronized", "thread_local",
    "transaction_safe", "transaction_safe_dynamic", "pre", "post",
};

const std::unordered_set<std::string> kTypes = {
    "bool", "char", "char8_t", "char16_t", "char32_t", "double", "float",
    "int", "long", "short", "signed", "unsigned", "void", "wchar_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "size_t", "ptrdiff_t", "intptr_t", "uintptr_t",
    "string", "vector", "map", "unordered_map", "set", "unordered_set",
    "array", "deque", "list", "pair", "tuple", "optional", "variant",
    "shared_ptr", "unique_ptr", "weak_ptr",
    "string_view", "span", "expected",
};

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c)  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool IsDigit(char c)      { return std::isdigit(static_cast<unsigned char>(c)); }
bool IsHexDigit(char c)   { return std::isxdigit(static_cast<unsigned char>(c)); }

}  // namespace

LexerState LexerCpp::TokenizeLine(std::string_view line,
                                   const LexerState& state_in,
                                   std::vector<Token>& out) const {
    out.clear();
    int i   = 0;
    int len = static_cast<int>(line.size());
    LexerState state = state_in;

    // ── Continue block comment from previous line ─────────────────────────
    if (state.state == 1) {
        int start = i;
        while (i < len) {
            if (i + 1 < len && line[i] == '*' && line[i + 1] == '/') {
                i += 2;
                out.push_back({TokenType::Comment, start, i - start});
                state.state = 0;
                goto normal;
            }
            ++i;
        }
        out.push_back({TokenType::Comment, start, i - start});
        return state;  // Still inside block comment.
    }

normal:
    while (i < len) {
        char c = line[i];

        // ── Whitespace ────────────────────────────────────────────────────
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        // ── Single-line comment (//) ──────────────────────────────────────
        if (i + 1 < len && c == '/' && line[i + 1] == '/') {
            out.push_back({TokenType::Comment, i, len - i});
            return {0};
        }

        // ── Block comment start (/*) ──────────────────────────────────────
        if (i + 1 < len && c == '/' && line[i + 1] == '*') {
            int start = i;
            i += 2;
            while (i + 1 < len) {
                if (line[i] == '*' && line[i + 1] == '/') {
                    i += 2;
                    out.push_back({TokenType::Comment, start, i - start});
                    goto normal;
                }
                ++i;
            }
            // Check the very last char pair.
            if (i < len) ++i;
            out.push_back({TokenType::Comment, start, i - start});
            state.state = 1;
            return state;
        }

        // ── Preprocessor directive ────────────────────────────────────────
        if (c == '#') {
            int hash_pos = i;
            // Tokenize # and the directive name (e.g. #include, #define, #ifdef, #undef)
            int d_start = i;
            ++i;
            while (i < len && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            int name_start = i;
            while (i < len && IsIdentChar(line[i])) ++i;
            std::string_view dir_name = line.substr(name_start, i - name_start);
            out.push_back({TokenType::Preprocessor, d_start, i - d_start});

            // If it's an include directive, parse the header path
            if (dir_name == "include") {
                while (i < len) {
                    while (i < len && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
                    if (i >= len) break;
                    
                    if (line[i] == '<') {
                        int h_start = i++;
                        while (i < len && line[i] != '>') ++i;
                        if (i < len) ++i; // include '>'
                        out.push_back({TokenType::String, h_start, i - h_start});
                    } else if (line[i] == '"') {
                        int h_start = i++;
                        while (i < len && line[i] != '"') ++i;
                        if (i < len) ++i; // include '"'
                        out.push_back({TokenType::String, h_start, i - h_start});
                    } else if (i + 1 < len && line[i] == '/' && line[i + 1] == '/') {
                        out.push_back({TokenType::Comment, i, len - i});
                        return {0};
                    } else {
                        ++i;
                    }
                }
                return {0};
            } else if (dir_name == "pragma") {
                // Check if the next token is "once"
                while (i < len && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
                int pragma_arg_start = i;
                while (i < len && IsIdentChar(line[i])) ++i;
                if (i > pragma_arg_start) {
                    std::string_view pragma_arg = line.substr(pragma_arg_start, i - pragma_arg_start);
                    if (pragma_arg == "once") {
                        out.push_back({TokenType::Keyword, pragma_arg_start, i - pragma_arg_start});
                    } else {
                        out.push_back({TokenType::Identifier, pragma_arg_start, i - pragma_arg_start});
                    }
                }
                continue;
            }
            continue;
        }

        // ── String literal ────────────────────────────────────────────────
        if (c == '"') {
            int start = i++;
            while (i < len) {
                if (line[i] == '\\' && i + 1 < len) { i += 2; continue; }
                if (line[i] == '"') { ++i; break; }
                ++i;
            }
            out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // ── Character literal ─────────────────────────────────────────────
        if (c == '\'') {
            int start = i++;
            while (i < len) {
                if (line[i] == '\\' && i + 1 < len) { i += 2; continue; }
                if (line[i] == '\'') { ++i; break; }
                ++i;
            }
            out.push_back({TokenType::Character, start, i - start});
            continue;
        }

        // ── Number ────────────────────────────────────────────────────────
        if (IsDigit(c) || (c == '.' && i + 1 < len && IsDigit(line[i + 1]))) {
            int start = i;
            if (c == '0' && i + 1 < len) {
                char next = line[i + 1];
                if (next == 'x' || next == 'X') {
                    i += 2;
                    while (i < len && (IsHexDigit(line[i]) || line[i] == '\'')) ++i;
                } else if (next == 'b' || next == 'B') {
                    i += 2;
                    while (i < len && (line[i] == '0' || line[i] == '1' || line[i] == '\'')) ++i;
                } else {
                    ++i;
                    while (i < len && (IsDigit(line[i]) || line[i] == '.' || line[i] == 'e' ||
                                       line[i] == 'E' || line[i] == '\'' || line[i] == '+' || line[i] == '-'))
                        ++i;
                }
            } else {
                while (i < len && (IsDigit(line[i]) || line[i] == '.' || line[i] == 'e' ||
                                   line[i] == 'E' || line[i] == '\'' || line[i] == '+' || line[i] == '-'))
                    ++i;
            }
            // Consume suffixes (u, l, f, etc.).
            while (i < len && IsIdentChar(line[i])) ++i;
            out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // ── Identifier / keyword / type ───────────────────────────────────
        if (IsIdentStart(c)) {
            int start = i;
            while (i < len && IsIdentChar(line[i])) ++i;
            std::string word(line.substr(start, i - start));

            TokenType type = TokenType::Identifier;
            if (kKeywords.contains(word))   type = TokenType::Keyword;
            else if (kTypes.contains(word)) type = TokenType::Type;
            // Heuristic: if followed by '(' it's likely a function call.
            else {
                int j = i;
                while (j < len && line[j] == ' ') ++j;
                if (j < len && line[j] == '(') type = TokenType::Function;
            }
            out.push_back({type, start, i - start});
            continue;
        }

        // ── Operators (multi-char) ────────────────────────────────────────
        if (i + 1 < len) {
            std::string_view two = line.substr(i, 2);
            if (two == "::" || two == "->" || two == "==" || two == "!=" ||
                two == "<=" || two == ">=" || two == "&&" || two == "||" ||
                two == "++" || two == "--" || two == "+=" || two == "-=" ||
                two == "*=" || two == "/=" || two == "%=" || two == "&=" ||
                two == "|=" || two == "^=" || two == "<<" || two == ">>") {
                out.push_back({TokenType::Operator, i, 2});
                i += 2;
                continue;
            }
        }

        // ── Single-char operator or punctuation ───────────────────────────
        if (c == '{' || c == '}' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == ';' || c == ',' || c == '.') {
            out.push_back({TokenType::Punctuation, i, 1});
        } else {
            out.push_back({TokenType::Operator, i, 1});
        }
        ++i;
    }

    return {0};
}

}  // namespace luce
