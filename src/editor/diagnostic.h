#pragma once
#include <string>
#include <vector>

namespace luce {

enum class DiagnosticSeverity { Error, Warning, Info };

struct Diagnostic {
    std::string file_path;
    int line;      // 1-indexed
    int column;    // 1-indexed
    std::string message;
    DiagnosticSeverity severity;
};

class DiagnosticManager {
public:
    static DiagnosticManager& Instance();
    
    void AddDiagnostic(const Diagnostic& diag);
    void ClearDiagnostics();
    void ClearDiagnosticsForFile(const std::string& file_path);
    void SetDiagnosticsForFile(const std::string& file_path, const std::vector<Diagnostic>& diags);
    std::vector<Diagnostic> GetDiagnosticsForFile(const std::string& file_path) const;
    const std::vector<Diagnostic>& GetDiagnostics() const;

private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace luce
