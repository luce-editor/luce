#include "editor/symbol_index.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <thread>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace luce {

SymbolIndex::SymbolIndex() = default;
SymbolIndex::~SymbolIndex() = default;

void SymbolIndex::Clear() {
    std::unique_lock lock(mutex_);
    symbols_by_name_.clear();
    indexed_file_sizes_.clear();
    indexed_dirs_.clear();
    currently_indexing_.clear();
}

size_t SymbolIndex::SymbolCount() const {
    std::shared_lock lock(mutex_);
    size_t count = 0;
    for (const auto& [name, list] : symbols_by_name_) {
        count += list.size();
    }
    return count;
}

void SymbolIndex::IndexFile(const std::string& file_path) {
    if (file_path.empty() || !fs::exists(file_path)) return;

    {
        std::unique_lock lock(mutex_);
        if (currently_indexing_.contains(file_path)) return;
        currently_indexing_.insert(file_path);
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::unique_lock lock(mutex_);
        currently_indexing_.erase(file_path);
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    IndexContent(file_path, content);

    {
        std::unique_lock lock(mutex_);
        currently_indexing_.erase(file_path);
    }
}

void SymbolIndex::IndexContent(const std::string& file_path, const std::string& content) {
    std::vector<SymbolInfo> file_symbols;
    std::string ext = fs::path(file_path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".lua") {
        ParseLuaContent(file_path, content, file_symbols);
    } else {
        ParseCppContent(file_path, content, file_symbols);
    }

    std::unique_lock lock(mutex_);
    // Remove previous symbols from this file
    for (auto it = symbols_by_name_.begin(); it != symbols_by_name_.end(); ) {
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [&file_path](const SymbolInfo& s) { return s.file_path == file_path; }), vec.end());
        if (vec.empty()) {
            it = symbols_by_name_.erase(it);
        } else {
            ++it;
        }
    }

    // Insert new symbols
    for (auto& s : file_symbols) {
        symbols_by_name_[s.name].push_back(std::move(s));
    }
    indexed_file_sizes_[file_path] = content.size();
}

void SymbolIndex::IndexDirectoryAsync(const std::string& root_dir) {
    if (root_dir.empty() || !fs::exists(root_dir)) return;

    {
        std::unique_lock lock(mutex_);
        if (indexed_dirs_.contains(root_dir)) return;
        indexed_dirs_.insert(root_dir);
    }

    std::thread([this, root_dir]() {
        std::vector<std::string> paths;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(
                     root_dir, fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".h" || ext == ".hpp" || ext == ".cpp" ||
                    ext == ".c" || ext == ".inl" || ext == ".lua") {
                    // Skip build / node_modules / git dirs
                    std::string p = entry.path().generic_string();
                    if (p.find("/build/") != std::string::npos ||
                        p.find("/_deps/") != std::string::npos ||
                        p.find("/.git/") != std::string::npos ||
                        p.find("/node_modules/") != std::string::npos) {
                        continue;
                    }
                    paths.push_back(p);
                }
            }
        } catch (...) {
            // Ignore filesystem errors during directory iteration
        }

        for (const auto& path : paths) {
            IndexFile(path);
        }
    }).detach();
}

static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void SymbolIndex::ParseCppContent(const std::string& file_path, const std::string& content,
                                  std::vector<SymbolInfo>& out_symbols) {
    std::istringstream stream(content);
    std::string line_str;
    int line_num = 0;

    struct ScopeEntry {
        std::string name;
        int brace_depth = -1;
        bool is_class_or_struct = false;
    };
    std::vector<ScopeEntry> scope_stack;
    int current_brace_depth = 0;

    std::string pending_doc_comment;

    // Helper to get full scope string
    auto GetCurrentScope = [&scope_stack]() -> std::string {
        std::string full;
        for (size_t i = 0; i < scope_stack.size(); ++i) {
            if (i > 0) full += "::";
            full += scope_stack[i].name;
        }
        return full;
    };

    // Regexes for C++ parsing
    static const std::regex re_namespace(R"(^\s*namespace\s+([a-zA-Z0-9_:]+)\s*\{?)");
    static const std::regex re_class_struct(R"(^\s*(?:template\s*<[^>]*>\s*)?(class|struct)\s+(?:alignas\([^)]*\)\s+)?(?:LUCE_API\s+)?([a-zA-Z0-9_]+))");
    static const std::regex re_func(R"(^\s*(?:(inline|virtual|static|constexpr|explicit|friend)\s+)*([a-zA-Z0-9_:<>&*~]+(?:\s*<[^>]*>)?[\s*&]+)+([a-zA-Z0-9_~:]+)\s*\(([^;{]*)\)\s*(const)?\s*(noexcept)?\s*(?:override|final)?\s*(?:=\s*0|=\s*default|=\s*delete)?\s*(?:\{|;))");
    static const std::regex re_ctor_dtor(R"(^\s*(?:explicit\s+)?(~?[a-zA-Z0-9_]+)\s*\(([^;{]*)\)\s*(?:const)?\s*(?:noexcept)?\s*(?::\s*[^;{]+)?\s*(?:\{|;))");
    static const std::regex re_member_var(R"(^\s*(?:(static|const|constexpr|mutable)\s+)*([a-zA-Z0-9_:<>&*]+(?:\s*<[^>]*>)?[\s*&]+)+([a-zA-Z0-9_]+)\s*(?:=\s*[^;]+|\{[^}]*\})?\s*;)");
    static const std::regex re_include(R"(^\s*#\s*include\s*["<]([^">]+)[">])");

    while (std::getline(stream, line_str)) {
        line_num++;
        std::string trimmed = Trim(line_str);

        // Track /// doc comments
        if (trimmed.starts_with("///")) {
            std::string comment_body = trimmed.substr(3);
            if (comment_body.starts_with(" ")) comment_body = comment_body.substr(1);
            if (!pending_doc_comment.empty()) pending_doc_comment += "\n";
            pending_doc_comment += comment_body;
            continue;
        }

        // Single-line block comments /** ... */
        if (trimmed.starts_with("/**") && trimmed.ends_with("*/") && trimmed.size() >= 5) {
            pending_doc_comment = Trim(trimmed.substr(3, trimmed.size() - 5));
            continue;
        }

        if (trimmed.empty() || trimmed.starts_with("//")) {
            if (trimmed.empty()) {
                pending_doc_comment.clear();
            }
            continue;
        }

        // Count braces for scope management
        int line_open_braces = 0;
        int line_close_braces = 0;
        for (char ch : trimmed) {
            if (ch == '{') {
                current_brace_depth++;
                line_open_braces++;
                if (!scope_stack.empty() && scope_stack.back().brace_depth == -1) {
                    scope_stack.back().brace_depth = current_brace_depth;
                }
            } else if (ch == '}') {
                current_brace_depth--;
                line_close_braces++;
                while (!scope_stack.empty() && scope_stack.back().brace_depth != -1 &&
                       current_brace_depth < scope_stack.back().brace_depth) {
                    scope_stack.pop_back();
                }
            }
        }

        // Depth at the start of this line before any braces on this line were evaluated
        int depth_before_line = current_brace_depth - line_open_braces + line_close_braces;
        int base_depth = scope_stack.empty() ? 0 : scope_stack.back().brace_depth;

        // Check namespace
        std::smatch ns_match;
        if (std::regex_search(trimmed, ns_match, re_namespace)) {
            std::string ns_name = ns_match[1].str();
            bool has_brace = (trimmed.find('{') != std::string::npos);
            scope_stack.push_back({ns_name, has_brace ? current_brace_depth : -1, false});
            pending_doc_comment.clear();
            continue;
        }

        // Check class or struct
        std::smatch cs_match;
        if (std::regex_search(trimmed, cs_match, re_class_struct)) {
            std::string kind_str = cs_match[1].str();
            std::string type_name = cs_match[2].str();

            // Ignore forward declarations ending in ';' without '{'
            if (trimmed.find(';') == std::string::npos || trimmed.find('{') != std::string::npos) {
                SymbolInfo sym;
                sym.name = type_name;
                sym.scope = GetCurrentScope();
                sym.signature = (kind_str == "class" ? "class " : "struct ");
                if (!sym.scope.empty()) sym.signature += sym.scope + "::";
                sym.signature += type_name;
                sym.doc_comment = pending_doc_comment;
                sym.file_path = file_path;
                sym.line = line_num;
                sym.kind = (kind_str == "class" ? SymbolKind::Class : SymbolKind::Struct);

                out_symbols.push_back(std::move(sym));

                bool has_brace = (trimmed.find('{') != std::string::npos);
                scope_stack.push_back({type_name, has_brace ? current_brace_depth : -1, true});
                pending_doc_comment.clear();
                continue;
            }
        }

        // Only search for functions and member variables if we are NOT inside a function body or control block
        bool at_decl_level = (base_depth == -1) || (depth_before_line <= base_depth);

        // Check functions & methods
        std::smatch fn_match;
        if (at_decl_level && std::regex_search(trimmed, fn_match, re_func)) {
            std::string raw_name = fn_match[3].str();
            // Skip common control keywords that resemble functions
            if (raw_name == "if" || raw_name == "while" || raw_name == "for" ||
                raw_name == "switch" || raw_name == "catch" || raw_name == "sizeof") {
                pending_doc_comment.clear();
                continue;
            }

            std::string sym_name = raw_name;
            std::string extra_scope;
            auto colon_pos = raw_name.rfind("::");
            if (colon_pos != std::string::npos) {
                extra_scope = raw_name.substr(0, colon_pos);
                sym_name = raw_name.substr(colon_pos + 2);
            }

            SymbolInfo sym;
            sym.name = sym_name;
            sym.file_path = file_path;
            sym.line = line_num;
            sym.kind = SymbolKind::Function;
            sym.doc_comment = pending_doc_comment;

            std::string cur_scope = GetCurrentScope();
            if (!extra_scope.empty()) {
                sym.scope = cur_scope.empty() ? extra_scope : (cur_scope + "::" + extra_scope);
            } else {
                sym.scope = cur_scope;
            }

            // Build signature matching the style:
            // "inline bool luce::App::WantsQuit() const"
            std::string sig = trimmed;
            // Strip trailing opening brace or semicolon
            while (!sig.empty() && (sig.back() == '{' || sig.back() == ';' || sig.back() == ' ')) {
                sig.pop_back();
            }
            sig = Trim(sig);

            // If signature lacks the full scope name, inject it into the function identifier
            if (!sym.scope.empty() && sig.find(sym.scope + "::" + sym.name) == std::string::npos) {
                auto name_pos = sig.find(sym.name + "(");
                if (name_pos != std::string::npos) {
                    sig.replace(name_pos, sym.name.length(), sym.scope + "::" + sym.name);
                }
            }

            sym.signature = sig;
            out_symbols.push_back(std::move(sym));
            pending_doc_comment.clear();
            continue;
        }

        // Check constructors & destructors
        std::smatch cd_match;
        if (at_decl_level && std::regex_search(trimmed, cd_match, re_ctor_dtor)) {
            std::string cd_name = cd_match[1].str();
            std::string cur_scope = GetCurrentScope();
            std::string last_scope = scope_stack.empty() ? "" : scope_stack.back().name;
            bool is_ctor_dtor = (!last_scope.empty() && (cd_name == last_scope || cd_name == ("~" + last_scope))) ||
                                (cd_name.starts_with("~")) ||
                                (cd_name.find("::") != std::string::npos);

            if (is_ctor_dtor && cd_name != "if" && cd_name != "while" && cd_name != "for" && cd_name != "switch") {
                SymbolInfo sym;
                sym.name = cd_name;
                sym.file_path = file_path;
                sym.line = line_num;
                sym.kind = SymbolKind::Method;
                sym.doc_comment = pending_doc_comment;
                sym.scope = cur_scope;

                std::string params = cd_match[2].str();
                sym.signature = (cur_scope.empty() ? cd_name : (cur_scope + "::" + cd_name)) + "(" + params + ")";
                out_symbols.push_back(std::move(sym));
                pending_doc_comment.clear();
                continue;
            }
        }

        // Check member variables (ONLY inside class/struct, never namespaces or function bodies)
        if (!scope_stack.empty() && scope_stack.back().is_class_or_struct &&
            base_depth != -1 && depth_before_line == base_depth &&
            trimmed != "public:" && trimmed != "private:" && trimmed != "protected:") {
            std::smatch var_match;
            if (std::regex_search(trimmed, var_match, re_member_var)) {
                std::string var_name = var_match[3].str();
                if (var_name != "return" && var_name != "break" && var_name != "continue" &&
                    var_name != "default" && var_name != "public" && var_name != "private" && var_name != "protected") {
                    SymbolInfo sym;
                    sym.name = var_name;
                    sym.file_path = file_path;
                    sym.line = line_num;
                    sym.kind = SymbolKind::Variable;
                    sym.doc_comment = pending_doc_comment;
                    sym.scope = GetCurrentScope();
                    sym.signature = var_match[2].str() + " " + sym.scope + "::" + sym.name;
                    out_symbols.push_back(std::move(sym));
                    pending_doc_comment.clear();
                    continue;
                }
            }
        }

        // Clear pending doc comment on include directives
        std::smatch inc_match;
        if (std::regex_search(trimmed, inc_match, re_include)) {
            pending_doc_comment.clear();
            continue;
        }

        // If line is non-empty and didn't match doc or declaration, clear pending doc comment
        pending_doc_comment.clear();
    }
}

void SymbolIndex::ParseLuaContent(const std::string& file_path, const std::string& content,
                                  std::vector<SymbolInfo>& out_symbols) {
    std::istringstream stream(content);
    std::string line_str;
    int line_num = 0;
    std::string pending_doc;

    static const std::regex re_lua_fn(R"(^\s*function\s+([a-zA-Z0-9_.:]+)\s*\(([^)]*)\))");

    while (std::getline(stream, line_str)) {
        line_num++;
        std::string trimmed = Trim(line_str);

        if (trimmed.starts_with("---") || trimmed.starts_with("--")) {
            std::string c = trimmed.substr(trimmed.starts_with("---") ? 3 : 2);
            if (c.starts_with(" ")) c = c.substr(1);
            if (!pending_doc.empty()) pending_doc += "\n";
            pending_doc += c;
            continue;
        }

        if (trimmed.empty()) {
            pending_doc.clear();
            continue;
        }

        std::smatch match;
        if (std::regex_search(trimmed, match, re_lua_fn)) {
            std::string full_fn_name = match[1].str();
            std::string params = match[2].str();

            std::string fn_name = full_fn_name;
            std::string scope;
            auto sep = full_fn_name.find_first_of(":.");
            if (sep != std::string::npos) {
                scope = full_fn_name.substr(0, sep);
                fn_name = full_fn_name.substr(sep + 1);
            }

            SymbolInfo sym;
            sym.name = fn_name;
            sym.scope = scope;
            sym.signature = "function " + full_fn_name + "(" + params + ")";
            sym.doc_comment = pending_doc;
            sym.file_path = file_path;
            sym.line = line_num;
            sym.kind = SymbolKind::Function;

            out_symbols.push_back(std::move(sym));
            pending_doc.clear();
            continue;
        }

        pending_doc.clear();
    }
}

std::optional<SymbolInfo> SymbolIndex::FindSymbol(const std::string& name,
                                                 const std::string& context_file) const {
    std::shared_lock lock(mutex_);
    auto it = symbols_by_name_.find(name);
    if (it == symbols_by_name_.end() || it->second.empty()) {
        return std::nullopt;
    }

    const auto& list = it->second;

    if (context_file.empty()) {
        for (const auto& s : list) {
            if (!s.doc_comment.empty()) return s;
        }
        for (const auto& s : list) {
            auto ext = fs::path(s.file_path).extension().string();
            if (ext == ".h" || ext == ".hpp") return s;
        }
        return list.front();
    }

    // 1. If symbol is declared in the same file as the context, prefer it
    for (const auto& s : list) {
        if (s.file_path == context_file) return s;
    }

    // 2. Check same directory (sibling files e.g. test.hpp next to main.cpp)
    std::string context_dir = fs::path(context_file).parent_path().generic_string();
    for (const auto& s : list) {
        std::string s_dir = fs::path(s.file_path).parent_path().generic_string();
        if (!context_dir.empty() && s_dir == context_dir) {
            if (s.kind == SymbolKind::Variable) {
                auto ext = fs::path(s.file_path).extension().string();
                if (ext != ".h" && ext != ".hpp") continue;
            }
            return s;
        }
    }

    // 3. Check header/source pairing (e.g. app.cpp <-> app.h)
    std::string context_stem = fs::path(context_file).stem().string();
    for (const auto& s : list) {
        if (fs::path(s.file_path).stem().string() == context_stem) {
            return s;
        }
    }

    // 4. Check headers or non-variable symbols in the same directory tree
    for (const auto& s : list) {
        if (s.kind == SymbolKind::Variable) continue;
        std::string s_dir = fs::path(s.file_path).parent_path().generic_string();
        if (!context_dir.empty() && !s_dir.empty()) {
            if (context_dir.starts_with(s_dir) || s_dir.starts_with(context_dir)) {
                return s;
            }
        }
    }

    // Never return unrelated symbols from other project directories
    return std::nullopt;
}

std::vector<SymbolInfo> SymbolIndex::FindAllSymbols(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = symbols_by_name_.find(name);
    if (it != symbols_by_name_.end()) {
        return it->second;
    }
    return {};
}

std::vector<SymbolInfo> SymbolIndex::GetMembersOf(const std::string& type_name) const {
    std::shared_lock lock(mutex_);
    std::vector<SymbolInfo> results;
    if (type_name.empty()) return results;

    std::string norm_type = type_name;
    auto col = norm_type.rfind("::");
    std::string short_type = (col != std::string::npos) ? norm_type.substr(col + 2) : norm_type;

    for (const auto& [name, list] : symbols_by_name_) {
        for (const auto& s : list) {
            bool match = false;
            if (s.scope == norm_type || s.scope == short_type) {
                match = true;
            } else if (s.scope.ends_with("::" + short_type)) {
                match = true;
            }

            if (match) {
                // Ignore constructor and destructor
                if (s.name == short_type || s.name.starts_with("~")) continue;
                results.push_back(s);
            }
        }
    }
    return results;
}

}  // namespace luce
