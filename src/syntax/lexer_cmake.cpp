// ============================================================================
// LexerCMake — Implementation.
// ============================================================================

#include "lexer_cmake.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace luce {

namespace {

const std::unordered_set<std::string> kCMakeCommands = {
    "cmake_minimum_required", "project", "set", "unset", "message",
    "add_executable", "add_library", "add_subdirectory", "add_dependencies",
    "add_custom_command", "add_custom_target", "target_include_directories",
    "target_link_libraries", "target_compile_definitions", "target_compile_options",
    "target_sources", "find_package", "find_path", "find_library", "find_file",
    "include", "include_directories", "link_directories", "enable_testing",
    "add_test", "install", "list", "string", "file", "math", "option",
    "if", "elseif", "else", "endif", "foreach", "endforeach", "while",
    "endwhile", "function", "endfunction", "macro", "endmacro", "return",
    "fetchcontent_declare", "fetchcontent_makeavailable", "fetchcontent_populate"
};

const std::unordered_set<std::string> kCMakeKeywords = {
    "REQUIRED", "PUBLIC", "PRIVATE", "INTERFACE", "STATIC", "SHARED",
    "MODULE", "CONFIG", "STATUS", "WARNING", "FATAL_ERROR", "AUTHOR_WARNING",
    "SEND_ERROR", "ON", "OFF", "TRUE", "FALSE", "DEFINED", "NOT", "AND",
    "OR", "COMMAND", "OUTPUT", "DEPENDS", "WORKING_DIRECTORY", "COMMENT",
    "VERBATIM", "PROPERTIES", "WIN32", "APPLE", "UNIX", "MSVC"
};

bool IsIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

LexerState LexerCMake::TokenizeLine(std::string_view line,
                                    const LexerState& state_in,
                                    std::vector<Token>& out) const {
    out.clear();
    int i = 0;
    int len = static_cast<int>(line.size());

    while (i < len) {
        char c = line[i];

        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        // Comment (#)
        if (c == '#') {
            out.push_back({TokenType::Comment, i, len - i});
            return {0};
        }

        // Variable expansion (${VAR})
        if (c == '$' && i + 1 < len && (line[i + 1] == '{' || line[i + 1] == '<')) {
            int start = i;
            char close_ch = (line[i + 1] == '{') ? '}' : '>';
            i += 2;
            while (i < len && line[i] != close_ch) ++i;
            if (i < len) ++i;
            out.push_back({TokenType::Identifier, start, i - start});
            continue;
        }

        // String ("...")
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

        // Punctuation / brackets
        if (c == '(' || c == ')' || c == '{' || c == '}' || c == ';') {
            out.push_back({TokenType::Punctuation, i, 1});
            ++i;
            continue;
        }

        // Identifiers / Commands / Keywords
        if (IsIdentChar(c) || c == '.' || c == '-' || c == '/' || c == ':') {
            int start = i;
            while (i < len && (IsIdentChar(line[i]) || line[i] == '.' || line[i] == '-' || line[i] == '/' || line[i] == ':')) {
                ++i;
            }
            std::string word(line.substr(start, i - start));
            std::string lower_word = word;
            std::ranges::transform(lower_word, lower_word.begin(), ::tolower);

            if (kCMakeCommands.contains(lower_word)) {
                out.push_back({TokenType::Function, start, i - start});
            } else if (kCMakeKeywords.contains(word)) {
                out.push_back({TokenType::Keyword, start, i - start});
            } else {
                out.push_back({TokenType::Identifier, start, i - start});
            }
            continue;
        }

        out.push_back({TokenType::Operator, i, 1});
        ++i;
    }

    return {0};
}

} // namespace luce
