#pragma once
// ============================================================================
// SymbolIndex — Lightweight workspace symbol indexer for C++, C, and Lua.
//
// Indexes function signatures, classes, structs, and doc comments (///) to
// power Ctrl+Hover signatures and Ctrl+Click "Go to Definition" navigation.
// ============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <memory>
#include <optional>

namespace luce {

enum class SymbolKind {
    Function,
    Method,
    Class,
    Struct,
    Variable
};

struct SymbolInfo {
    std::string name;          ///< Unqualified symbol name (e.g. "WantsQuit")
    std::string scope;         ///< Enclosing namespace / class (e.g. "luce::App")
    std::string signature;     ///< Full declaration (e.g. "inline bool luce::App::WantsQuit() const")
    std::string doc_comment;   ///< Extracted doc comment (e.g. "Whether the app wants to quit...")
    std::string file_path;     ///< Absolute or normalized path to source file
    int         line = 1;      ///< 1-indexed line number
    SymbolKind  kind = SymbolKind::Function;
};

class SymbolIndex {
public:
    SymbolIndex();
    ~SymbolIndex();

    /// Index a single file from disk.
    void IndexFile(const std::string& file_path);

    /// Index source content directly (e.g. for unsaved / open buffers).
    void IndexContent(const std::string& file_path, const std::string& content);

    /// Recursively index all source/header files in a directory asynchronously.
    void IndexDirectoryAsync(const std::string& root_dir);

    /// Clear all indexed symbols.
    void Clear();

    /// Find the best matching symbol by identifier name.
    /// If `context_file` is provided, symbols in or related to that file are preferred.
    std::optional<SymbolInfo> FindSymbol(const std::string& name, const std::string& context_file = "") const;

    /// Get all symbols matching a name (e.g. for overloads / multiple classes).
    std::vector<SymbolInfo> FindAllSymbols(const std::string& name) const;

    /// Get all member functions and fields belonging to the specified type/class.
    std::vector<SymbolInfo> GetMembersOf(const std::string& type_name) const;

    /// Total number of indexed symbols.
    size_t SymbolCount() const;

private:
    void ParseCppContent(const std::string& file_path, const std::string& content, std::vector<SymbolInfo>& out_symbols);
    void ParseLuaContent(const std::string& file_path, const std::string& content, std::vector<SymbolInfo>& out_symbols);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<SymbolInfo>> symbols_by_name_;
    std::unordered_map<std::string, size_t> indexed_file_sizes_;
    std::unordered_set<std::string> indexed_dirs_;
    std::unordered_set<std::string> currently_indexing_;
};

}  // namespace luce
