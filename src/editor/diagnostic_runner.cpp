#include "diagnostic_runner.h"
#include "platform.h"

#include <regex>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace luce {

namespace {

std::string NormalizePath(std::string p) {
    std::ranges::replace(p, '\\', '/');
    return p;
}

}  // namespace

std::vector<Diagnostic> DiagnosticRunner::ParseCompilerOutput(const std::string& output, const std::string& target_file) {
    std::vector<Diagnostic> diagnostics;
    std::istringstream stream(output);
    std::string line;

    // GCC / Clang / Rustc: file:line:col: (error|warning|fatal error|note): message
    static const std::regex kGccRegex(R"(^([^:\r\n]+):(\d+):(\d+):\s+(fatal error|error|warning|note):\s+(.*)$)");

    // MSVC: file(line): error Cxxxx: message OR file(line,col): error Cxxxx: message
    static const std::regex kMsvcRegex(R"(^([^(:\r\n]+)\((\d+)(?:,(\d+))?\):\s+(fatal error|error|warning)\s+([A-Za-z0-9]+):\s+(.*)$)");

    // Python py_compile: File "path", line X
    static const std::regex kPyFileRegex("^\\s*File\\s+\"([^\"]+)\",\\s+line\\s+(\\d+)");
    static const std::regex kPyErrorRegex("^([A-Za-z0-9_]+Error):\\s*(.*)$");

    int py_pending_line = 0;
    std::string py_pending_file;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::smatch match;

        // Try GCC / Clang
        if (std::regex_search(line, match, kGccRegex)) {
            Diagnostic diag;
            diag.file_path = NormalizePath(match[1].str());
            diag.line = std::stoi(match[2].str());
            diag.column = std::stoi(match[3].str());

            std::string sev_str = match[4].str();
            std::ranges::transform(sev_str, sev_str.begin(), ::tolower);
            if (sev_str.find("error") != std::string::npos) {
                diag.severity = DiagnosticSeverity::Error;
            } else if (sev_str.find("warning") != std::string::npos) {
                diag.severity = DiagnosticSeverity::Warning;
            } else {
                diag.severity = DiagnosticSeverity::Info;
            }

            diag.message = match[5].str();
            diagnostics.push_back(std::move(diag));
            continue;
        }

        // Try MSVC
        if (std::regex_search(line, match, kMsvcRegex)) {
            Diagnostic diag;
            diag.file_path = NormalizePath(match[1].str());
            diag.line = std::stoi(match[2].str());
            diag.column = match[3].matched ? std::stoi(match[3].str()) : 1;

            std::string sev_str = match[4].str();
            std::ranges::transform(sev_str, sev_str.begin(), ::tolower);
            if (sev_str.find("error") != std::string::npos) {
                diag.severity = DiagnosticSeverity::Error;
            } else {
                diag.severity = DiagnosticSeverity::Warning;
            }

            diag.message = match[5].str() + ": " + match[6].str();
            diagnostics.push_back(std::move(diag));
            continue;
        }

        // Try Python py_compile multi-line format
        if (std::regex_search(line, match, kPyFileRegex)) {
            py_pending_file = NormalizePath(match[1].str());
            py_pending_line = std::stoi(match[2].str());
            continue;
        }

        if (py_pending_line > 0 && std::regex_search(line, match, kPyErrorRegex)) {
            Diagnostic diag;
            diag.file_path = py_pending_file.empty() ? target_file : py_pending_file;
            diag.line = py_pending_line;
            diag.column = 1;
            diag.severity = DiagnosticSeverity::Error;
            diag.message = match[1].str() + ": " + match[2].str();
            diagnostics.push_back(std::move(diag));

            py_pending_line = 0;
            py_pending_file.clear();
            continue;
        }
    }

    return diagnostics;
}

void DiagnosticRunner::CheckFile(const std::string& file_path, const std::string& working_dir) {
    if (file_path.empty()) return;

    std::string norm_path = NormalizePath(file_path);
    std::string ext = platform::GetExtension(file_path);
    std::ranges::transform(ext, ext.begin(), ::tolower);

    std::string cwd = working_dir.empty() ? platform::GetDirectory(file_path) : working_dir;
    std::vector<Diagnostic> diags;

    if (ext == ".py" || ext == ".pyw") {
        // Python syntax check
        std::string cmd = "python -m py_compile \"" + norm_path + "\"";
        auto res = platform::RunCommand(cmd, cwd);
        diags = ParseCompilerOutput(res.output, norm_path);
    } else if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c") {
        // C/C++ syntax check: try clang++ then g++ then cl
        std::string cmd = "clang++ -fsyntax-only -Wall -std=c++20 \"" + norm_path + "\"";
        auto res = platform::RunCommand(cmd, cwd);
        if (res.exit_code != 0 && res.output.empty()) {
            cmd = "g++ -fsyntax-only -Wall -std=c++20 \"" + norm_path + "\"";
            res = platform::RunCommand(cmd, cwd);
        }
        if (res.exit_code != 0 && res.output.empty()) {
            cmd = "cl.exe /Zs /std:c++20 /nologo \"" + norm_path + "\"";
            res = platform::RunCommand(cmd, cwd);
        }
        diags = ParseCompilerOutput(res.output, norm_path);
    } else if (ext == ".rs") {
        // Rust syntax check
        std::string cmd = "rustc --error-format=short \"" + norm_path + "\"";
        auto res = platform::RunCommand(cmd, cwd);
        diags = ParseCompilerOutput(res.output, norm_path);
    }

    // Fix up relative file paths if compiler printed relative names
    for (auto& d : diags) {
        if (!fs::path(d.file_path).is_absolute() && !norm_path.empty()) {
            if (NormalizePath(platform::GetFilename(d.file_path)) == NormalizePath(platform::GetFilename(norm_path))) {
                d.file_path = norm_path;
            }
        }
    }

    DiagnosticManager::Instance().SetDiagnosticsForFile(norm_path, diags);
}

}  // namespace luce
