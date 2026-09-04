// ============================================================================
// LexerRust — Hand-written tokeniser for Rust source files.
//
// Lexer states:
//   0 = normal
//   1 = inside a block comment (/* … */), with nesting depth encoded in
//       the upper bits  (state = 1 | (depth << 8))
// ============================================================================

#include "lexer_rust.h"
#include <unordered_set>

namespace luce {

namespace {

const std::unordered_set<std::string> kKeywords = {
    "as", "async", "await", "break", "const", "continue", "crate", "dyn",
    "else", "enum", "extern", "false", "fn", "for", "if", "impl", "in",
    "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
    "self", "Self", "static", "struct", "super", "trait", "true", "type",
    "union", "unsafe", "use", "where", "while", "yield",
};

const std::unordered_set<std::string> kTypes = {
    "i8", "i16", "i32", "i64", "i128", "isize",
    "u8", "u16", "u32", "u64", "u128", "usize",
    "f32", "f64", "bool", "char", "str",
    "String", "Vec", "Box", "Rc", "Arc", "Cell", "RefCell",
    "Option", "Result", "Some", "None", "Ok", "Err",
    "HashMap", "HashSet", "BTreeMap", "BTreeSet",
    "Cow", "Pin", "PhantomData",
};

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c)  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool IsDigit(char c)      { return std::isdigit(static_cast<unsigned char>(c)); }

}  // namespace

LexerState LexerRust::TokenizeLine(std::string_view line,
                                    const LexerState& state_in,
                                    std::vector<Token>& out) const {
    out.clear();
    int i     = 0;
    int len   = static_cast<int>(line.size());
    int state = state_in.state;

    // ── Block comment continuation (with nesting support) ─────────────────
    if ((state & 0xFF) == 1) {
        int depth = state >> 8;
        int start = i;
        while (i < len) {
            if (i + 1 < len && line[i] == '/' && line[i + 1] == '*') {
                ++depth; i += 2; continue;
            }
            if (i + 1 < len && line[i] == '*' && line[i + 1] == '/') {
                --depth; i += 2;
                if (depth == 0) {
                    out.push_back({TokenType::Comment, start, i - start});
                    state = 0;
                    goto normal;
                }
                continue;
            }
            ++i;
        }
        out.push_back({TokenType::Comment, start, i - start});
        return {1 | (depth << 8)};
    }

normal:
    while (i < len) {
        char c = line[i];

        // Whitespace.
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }

        // Line comment (// …).
        if (i + 1 < len && c == '/' && line[i + 1] == '/') {
            out.push_back({TokenType::Comment, i, len - i});
            return {0};
        }

        // Block comment start (/* …) with nesting.
        if (i + 1 < len && c == '/' && line[i + 1] == '*') {
            int depth = 1, start = i;
            i += 2;
            while (i < len) {
                if (i + 1 < len && line[i] == '/' && line[i + 1] == '*') { ++depth; i += 2; continue; }
                if (i + 1 < len && line[i] == '*' && line[i + 1] == '/') {
                    --depth; i += 2;
                    if (depth == 0) { out.push_back({TokenType::Comment, start, i - start}); goto normal; }
                    continue;
                }
                ++i;
            }
            out.push_back({TokenType::Comment, start, i - start});
            return {1 | (depth << 8)};
        }

        // Attribute: #[…] or #![…].
        if (c == '#' && i + 1 < len && (line[i + 1] == '[' || (line[i + 1] == '!' && i + 2 < len && line[i + 2] == '['))) {
            int start = i;
            int bracket_depth = 0;
            while (i < len) {
                if (line[i] == '[') ++bracket_depth;
                if (line[i] == ']') { --bracket_depth; if (bracket_depth == 0) { ++i; break; } }
                ++i;
            }
            out.push_back({TokenType::Attribute, start, i - start});
            continue;
        }

        // Lifetime: 'a, 'static, etc.
        if (c == '\'' && i + 1 < len && IsIdentStart(line[i + 1])) {
            int start = i++;
            while (i < len && IsIdentChar(line[i])) ++i;
            out.push_back({TokenType::Lifetime, start, i - start});
            continue;
        }

        // Character literal: 'x' or '\n'.
        if (c == '\'') {
            int start = i++;
            if (i < len && line[i] == '\\') i += 2;
            else if (i < len) ++i;
            if (i < len && line[i] == '\'') ++i;
            out.push_back({TokenType::Character, start, i - start});
            continue;
        }

        // Raw string: r#"…"# (with variable number of '#').
        if (c == 'r' && i + 1 < len && (line[i + 1] == '"' || line[i + 1] == '#')) {
            int start = i++;
            int hashes = 0;
            while (i < len && line[i] == '#') { ++hashes; ++i; }
            if (i < len && line[i] == '"') {
                ++i;
                while (i < len) {
                    if (line[i] == '"') {
                        int end_hashes = 0;
                        int j = i + 1;
                        while (j < len && line[j] == '#' && end_hashes < hashes) { ++end_hashes; ++j; }
                        if (end_hashes == hashes) { i = j; break; }
                    }
                    ++i;
                }
                out.push_back({TokenType::String, start, i - start});
                continue;
            }
            // Not a raw string; back up and treat 'r' as identifier.
            i = start;
        }

        // Byte string: b"…" or b'x'.
        if (c == 'b' && i + 1 < len && (line[i + 1] == '"' || line[i + 1] == '\'')) {
            int start = i++;
            char delim = line[i++];
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == delim) { ++i; break; }
                ++i;
            }
            out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // String literal: "…".
        if (c == '"') {
            int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == '"') { ++i; break; }
                ++i;
            }
            out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // Number.
        if (IsDigit(c)) {
            int start = i;
            if (c == '0' && i + 1 < len) {
                char next = line[i + 1];
                if (next == 'x' || next == 'o' || next == 'b') {
                    i += 2;
                    while (i < len && (IsIdentChar(line[i]) || line[i] == '_')) ++i;
                    out.push_back({TokenType::Number, start, i - start});
                    continue;
                }
            }
            while (i < len && (IsDigit(line[i]) || line[i] == '_' || line[i] == '.' ||
                               line[i] == 'e' || line[i] == 'E')) ++i;
            // Suffix (u32, f64, etc.).
            while (i < len && IsIdentChar(line[i])) ++i;
            out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // Identifier / keyword / type / macro.
        if (IsIdentStart(c)) {
            int start = i;
            while (i < len && IsIdentChar(line[i])) ++i;
            std::string word(line.substr(start, i - start));

            // Macro invocation: word followed by '!'.
            if (i < len && line[i] == '!') {
                ++i;  // consume '!'
                out.push_back({TokenType::Macro, start, i - start});
                continue;
            }

            TokenType tt = TokenType::Identifier;
            if (kKeywords.contains(word))   tt = TokenType::Keyword;
            else if (kTypes.contains(word)) tt = TokenType::Type;
            else {
                // Heuristic: uppercase start → likely a type.
                if (std::isupper(static_cast<unsigned char>(word[0]))) tt = TokenType::Type;
                // Heuristic: followed by '(' → function call.
                int j = i;
                while (j < len && line[j] == ' ') ++j;
                if (j < len && line[j] == '(' && tt == TokenType::Identifier) tt = TokenType::Function;
            }
            out.push_back({tt, start, i - start});
            continue;
        }

        // Multi-char operators.
        if (i + 1 < len) {
            std::string_view two = line.substr(i, 2);
            if (two == "::" || two == "->" || two == "=>" || two == "==" ||
                two == "!=" || two == "<=" || two == ">=" || two == "&&" ||
                two == "||" || two == ".." || two == "+=" || two == "-=" ||
                two == "*=" || two == "/=" || two == "<<" || two == ">>") {
                out.push_back({TokenType::Operator, i, 2});
                i += 2;
                continue;
            }
        }

        // Punctuation.
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
