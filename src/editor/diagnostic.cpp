#include "diagnostic.h"

namespace luce {

DiagnosticManager& DiagnosticManager::Instance() {
    static DiagnosticManager instance;
    return instance;
}

void DiagnosticManager::AddDiagnostic(const Diagnostic& diag) {
    diagnostics_.push_back(diag);
}

void DiagnosticManager::ClearDiagnostics() {
    diagnostics_.clear();
}

void DiagnosticManager::ClearDiagnosticsForFile(const std::string& file_path) {
    std::erase_if(diagnostics_, [&](const Diagnostic& d) {
        return d.file_path == file_path;
    });
}

void DiagnosticManager::SetDiagnosticsForFile(const std::string& file_path, const std::vector<Diagnostic>& diags) {
    ClearDiagnosticsForFile(file_path);
    for (const auto& d : diags) {
        diagnostics_.push_back(d);
    }
}

std::vector<Diagnostic> DiagnosticManager::GetDiagnosticsForFile(const std::string& file_path) const {
    std::vector<Diagnostic> result;
    for (const auto& d : diagnostics_) {
        if (d.file_path == file_path) {
            result.push_back(d);
        }
    }
    return result;
}

const std::vector<Diagnostic>& DiagnosticManager::GetDiagnostics() const {
    return diagnostics_;
}

} // namespace luce
