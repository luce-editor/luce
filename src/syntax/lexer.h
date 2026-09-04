#pragma once
// ============================================================================
// Lexer — Abstract tokeniser interface and token types.
//
// Every language lexer (C++, Rust, Web, …) inherits from Lexer and implements
// TokenizeLine().  The SyntaxHighlighter maps each TokenType to a colour
// defined by the active theme.
// ============================================================================

#include <string>
#include <string_view>
#include <vector>

namespace luce {

/// Universal token categories shared across all languages.
/// Each lexer maps its language-specific constructs to these categories so that
/// the theme can colour them consistently.
enum class TokenType {
    None,            ///< Default / unclassified text.
    Keyword,         ///< Language keywords (if, else, for, fn, let, …).
    Type,            ///< Type names (int, String, Vec, …).
    String,          ///< String literals.
    Character,       ///< Character literals ('a', '\n').
    Number,          ///< Numeric literals (42, 0xFF, 3.14, …).
    Comment,         ///< Single-line and multi-line comments.
    Preprocessor,    ///< Preprocessor directives (#include, #define).
    Operator,        ///< Operators (+, -, ==, ->, …).
    Punctuation,     ///< Brackets, semicolons, commas.
    Function,        ///< Function / method names at call sites.
    Identifier,      ///< Regular identifiers.
    Namespace,       ///< Namespace / module names.
    Macro,           ///< Macro invocations (println!, vec!).
    Attribute,       ///< Attributes / annotations (#[derive(…)], [[nodiscard]]).
    Tag,             ///< HTML/XML tag names (<div>, <span>).
    TagBracket,      ///< The angle brackets around tags (< > />).
    Property,        ///< CSS properties, HTML attributes.
    Value,           ///< CSS values, HTML attribute values.
    Lifetime,        ///< Rust lifetime annotations ('a).
    Escape,          ///< Escape sequences inside strings (\n, \x41).
};

/// A single token produced by a lexer.
struct Token {
    TokenType   type   = TokenType::None;
    int         start  = 0;   ///< Column offset (0-based) within the line.
    int         length = 0;   ///< Number of characters.
};

/// Per-line state carried between consecutive calls to TokenizeLine(),
/// needed to correctly handle multi-line constructs (block comments,
/// multi-line strings, embedded languages, …).
struct LexerState {
    int state = 0;   ///< 0 = normal; higher values are lexer-specific.
};

/// Abstract base class for all language lexers.
///
/// A lexer examines a single line of source code and produces a list of
/// `Token` structs.  It also receives and returns a `LexerState` so that
/// multi-line constructs (block comments, heredocs, …) are handled correctly.
class Lexer {
public:
    virtual ~Lexer() = default;

    /// Tokenise a single line.  `state_in` carries the state from the
    /// preceding line (or default-constructed for line 0).  Returns the
    /// state that should be passed to the next line.
    virtual LexerState TokenizeLine(
        std::string_view            line,
        const LexerState&           state_in,
        std::vector<Token>&         tokens_out) const = 0;

    /// Human-readable language name (e.g. "C++", "Rust", "HTML").
    virtual const char* GetLanguageName() const = 0;

    /// File extensions this lexer handles (e.g. {".cpp", ".hpp", ".h"}).
    virtual std::vector<std::string> GetExtensions() const = 0;
};

}  // namespace luce
