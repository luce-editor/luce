#pragma once
// ============================================================================
// DiagnosticRunner — Real problem detection and linter engine.
//
// Automatically runs compiler syntax checks (Clang/GCC/MSVC for C++,
// py_compile for Python, rustc/cargo for Rust) and populates the
// DiagnosticManager with real errors and warnings.
// ============================================================================

#include "diagnostic.h"
#include <string>
#include <vector>

namespace luce {

class DiagnosticRunner {
public:
    static DiagnosticRunner& Instance() {
        static DiagnosticRunner instance;
        return instance;
    }

    /// Check a file for syntax / compilation errors asynchronously or synchronously.
    void CheckFile(const std::string& file_path, const std::string& working_dir = "");

    /// Parse compiler output text into Diagnostic structs.
    static std::vector<Diagnostic> ParseCompilerOutput(const std::string& output, const std::string& target_file);

private:
    DiagnosticRunner() = default;
};

}  // namespace luce
