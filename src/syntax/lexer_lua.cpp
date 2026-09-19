#include "lexer_lua.h"

#include <unordered_set>
#include <string>

namespace luce {

namespace {

const std::unordered_set<std::string_view> kKeywords = {
    "and", "break", "do", "else", "elseif", "end", "false", "for",
    "function", "goto", "if", "in", "local", "nil", "not", "or",
    "repeat", "return", "then", "true", "until", "while"
};

const std::unordered_set<std::string_view> kTypes = {
    "boolean", "number", "string", "table", "function", "thread", "userdata"
};

const std::unordered_set<std::string_view> kStdLibs = {
    "string", "table", "math", "io", "os", "debug", "utf8", "package", "_G", "_VERSION"
};

const std::unordered_set<std::string_view> kStdFunctions = {
    "assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs",
    "load", "loadfile", "next", "pairs", "pcall", "print", "rawequal",
    "rawget", "rawlen", "rawset", "require", "select", "setmetatable",
    "tonumber", "tostring", "type", "warn", "xpcall"
};

// Luce Editor API functions and tables
const std::unordered_set<std::string_view> kLuceAPI = {
    "register_command", "on", "get_buffer_text", "set_buffer_text",
    "get_line_count", "get_line", "set_line", "insert_line", "delete_line",
    "get_cursor", "set_cursor", "get_selection", "set_selection",
    "insert_at_cursor", "get_active_file", "open_file", "save_active",
    "get_workspace_path", "execute_command", "add_diagnostic", "clear_diagnostics",
    "register_completion_provider", "set_status", "show_notification",
    "show_error", "show_warning", "show_info", "log", "warn", "plugin"
};

bool IsIdentStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool IsIdentChar(char c) {
    return IsIdentStart(c) || (c >= '0' && c <= '9');
}

}  // namespace

LexerState LexerLua::TokenizeLine(std::string_view line, const LexerState& state_in,
                                  std::vector<Token>& tokens_out) const {
    LexerState state_out = state_in;
    int i = 0;
    int len = static_cast<int>(line.length());

    // State 1: Inside multi-line block comment --[[ ... ]]
    if (state_out.state == 1) {
        size_t close_pos = line.find("]]");
        if (close_pos != std::string_view::npos) {
            int end_idx = static_cast<int>(close_pos) + 2;
            tokens_out.push_back({TokenType::Comment, 0, end_idx});
            i = end_idx;
            state_out.state = 0;
        } else {
            tokens_out.push_back({TokenType::Comment, 0, len});
            return state_out;
        }
    }

    // State 2: Inside multi-line string literal [[ ... ]]
    if (state_out.state == 2) {
        size_t close_pos = line.find("]]");
        if (close_pos != std::string_view::npos) {
            int end_idx = static_cast<int>(close_pos) + 2;
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

        // Comments: -- or --[[
        if (c == '-' && i + 1 < len && line[i + 1] == '-') {
            // Check for multi-line block comment --[[
            if (i + 3 < len && line[i + 2] == '[' && line[i + 3] == '[') {
                int start = i;
                size_t close_pos = line.find("]]", i + 4);
                if (close_pos != std::string_view::npos) {
                    int end_idx = static_cast<int>(close_pos) + 2;
                    tokens_out.push_back({TokenType::Comment, start, end_idx - start});
                    i = end_idx;
                } else {
                    tokens_out.push_back({TokenType::Comment, start, len - start});
                    state_out.state = 1;
                    return state_out;
                }
                continue;
            }

            // Single line comment
            tokens_out.push_back({TokenType::Comment, i, len - i});
            break;
        }

        // Multi-line string literal [[ ... ]]
        if (c == '[' && i + 1 < len && line[i + 1] == '[') {
            int start = i;
            size_t close_pos = line.find("]]", i + 2);
            if (close_pos != std::string_view::npos) {
                int end_idx = static_cast<int>(close_pos) + 2;
                tokens_out.push_back({TokenType::String, start, end_idx - start});
                i = end_idx;
            } else {
                tokens_out.push_back({TokenType::String, start, len - start});
                state_out.state = 2;
                return state_out;
            }
            continue;
        }

        // Single or double quoted strings
        if (c == '"' || c == '\'') {
            char quote = c;
            int start = i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) {
                    i += 2;
                } else {
                    ++i;
                }
            }
            if (i < len && line[i] == quote) {
                ++i;
            }
            tokens_out.push_back({TokenType::String, start, i - start});
            continue;
        }

        // Hexadecimal and Decimal Numbers
        if (c == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
            int start = i;
            i += 2;
            while (i < len && (std::isxdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' ||
                               line[i] == 'p' || line[i] == 'P' ||
                               ((line[i] == '+' || line[i] == '-') && i > start && (line[i - 1] == 'p' || line[i - 1] == 'P')))) {
                ++i;
            }
            tokens_out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < len && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            int start = i++;
            while (i < len && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' ||
                               line[i] == 'e' || line[i] == 'E' ||
                               ((line[i] == '+' || line[i] == '-') && i > start && (line[i - 1] == 'e' || line[i - 1] == 'E')))) {
                ++i;
            }
            tokens_out.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // Identifiers, Keywords, and Functions
        if (IsIdentStart(c)) {
            int start = i++;
            while (i < len && IsIdentChar(line[i])) {
                ++i;
            }
            std::string_view word = line.substr(start, i - start);

            TokenType ttype = TokenType::Identifier;
            if (kKeywords.contains(word)) {
                ttype = TokenType::Keyword;
            } else if (kTypes.contains(word)) {
                ttype = TokenType::Type;
            } else if (word == "luce") {
                ttype = TokenType::Namespace;
            } else if (kLuceAPI.contains(word)) {
                // Check if preceded by luce.
                int prev = start - 1;
                while (prev >= 0 && (line[prev] == ' ' || line[prev] == '\t')) --prev;
                if (prev >= 0 && line[prev] == '.') {
                    int mod_end = prev;
                    int mod_start = mod_end;
                    while (mod_start > 0 && IsIdentChar(line[mod_start - 1])) --mod_start;
                    if (line.substr(mod_start, mod_end - mod_start) == "luce") {
                        ttype = (word == "plugin") ? TokenType::Property : TokenType::Function;
                    } else {
                        ttype = TokenType::Function;
                    }
                } else {
                    ttype = (word == "plugin") ? TokenType::Property : TokenType::Function;
                }
            } else if (kStdFunctions.contains(word)) {
                ttype = TokenType::Function;
            } else if (kStdLibs.contains(word)) {
                ttype = TokenType::Namespace;
            } else {
                // Lookahead for function call: foo(...)
                int peek = i;
                while (peek < len && (line[peek] == ' ' || line[peek] == '\t')) ++peek;
                if (peek < len && (line[peek] == '(' || line[peek] == '{' || line[peek] == '"' || line[peek] == '\'')) {
                    ttype = TokenType::Function;
                }
            }

            tokens_out.push_back({ttype, start, i - start});
            continue;
        }

        // Multi-character operators
        if (i + 2 < len && line.substr(i, 3) == "...") {
            tokens_out.push_back({TokenType::Operator, i, 3});
            i += 3;
            continue;
        }

        if (i + 1 < len) {
            std::string_view op2 = line.substr(i, 2);
            if (op2 == "==" || op2 == "~=" || op2 == "<=" || op2 == ">=" ||
                op2 == "//" || op2 == ".." || op2 == "<<" || op2 == ">>" || op2 == "::") {
                tokens_out.push_back({TokenType::Operator, i, 2});
                i += 2;
                continue;
            }
        }

        // Single-character operators
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '^' || c == '#' || c == '&' || c == '~' || c == '|' ||
            c == '<' || c == '>' || c == '=') {
            tokens_out.push_back({TokenType::Operator, i, 1});
            ++i;
            continue;
        }

        // Punctuation
        if (c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
            c == ';' || c == ':' || c == ',' || c == '.') {
            tokens_out.push_back({TokenType::Punctuation, i, 1});
            ++i;
            continue;
        }

        // Unclassified character
        tokens_out.push_back({TokenType::None, i, 1});
        ++i;
    }

    return state_out;
}

}  // namespace luce
