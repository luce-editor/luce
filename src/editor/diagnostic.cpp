#include "diagnostic.h"
#include <algorithm>

namespace luce {

namespace {
std::string NormalizeAndLower(std::string p) {
    std::ranges::replace(p, '\\', '/');
    std::ranges::transform(p, p.begin(), ::tolower);
    return p;
}
}

DiagnosticManager& DiagnosticManager::Instance() {
    static DiagnosticManager instance;
    return instance;
}

void DiagnosticManager::AddDiagnostic(const Diagnostic& diag) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.push_back(diag);
}

void DiagnosticManager::ClearDiagnostics() {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.clear();
}

void DiagnosticManager::ClearDiagnosticsForFile(const std::string& file_path) {
    std::string norm = NormalizeAndLower(file_path);
    std::lock_guard<std::mutex> lock(mutex_);
    std::erase_if(diagnostics_, [&](const Diagnostic& d) {
        std::string orig = NormalizeAndLower(d.origin_file.empty() ? d.file_path : d.origin_file);
        return orig == norm || NormalizeAndLower(d.file_path) == norm;
    });
}

void DiagnosticManager::SetDiagnosticsForFile(const std::string& file_path, const std::vector<Diagnostic>& diags) {
    std::string norm = NormalizeAndLower(file_path);
    std::lock_guard<std::mutex> lock(mutex_);
    // Remove all previous diagnostics that originated from compiling this file
    std::erase_if(diagnostics_, [&](const Diagnostic& d) {
        std::string orig = NormalizeAndLower(d.origin_file.empty() ? d.file_path : d.origin_file);
        return orig == norm;
    });
    for (const auto& d : diags) {
        bool duplicate = std::ranges::any_of(diagnostics_, [&](const Diagnostic& existing) {
            return NormalizeAndLower(existing.file_path) == NormalizeAndLower(d.file_path) &&
                   existing.line == d.line &&
                   existing.column == d.column &&
                   existing.message == d.message;
        });
        if (!duplicate) {
            diagnostics_.push_back(d);
        }
    }
}

std::vector<Diagnostic> DiagnosticManager::GetDiagnosticsForFile(const std::string& file_path) const {
    std::string norm = NormalizeAndLower(file_path);
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Diagnostic> result;
    for (const auto& d : diagnostics_) {
        if (NormalizeAndLower(d.file_path) == norm) {
            result.push_back(d);
        }
    }
    return result;
}

std::vector<Diagnostic> DiagnosticManager::GetDiagnostics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return diagnostics_;
}

} // namespace luce
