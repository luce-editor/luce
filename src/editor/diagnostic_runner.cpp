#include "diagnostic_runner.h"
#include "platform.h"

#include <regex>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iostream>

#include <thread>

namespace fs = std::filesystem;

namespace luce {

namespace {

std::string NormalizePath(std::string p) {
    std::ranges::replace(p, '\\', '/');
    return p;
}

bool IsCompilerMissing(const std::string& output) {
    if (output.empty()) return false;
    std::string lower = output;
    std::ranges::transform(lower, lower.begin(), ::tolower);
    return lower.find("not recognized") != std::string::npos ||
           lower.find("nie jest rozpoznawan") != std::string::npos ||
           lower.find("not found") != std::string::npos ||
           lower.find("cannot find") != std::string::npos ||
           lower.find("no such file") != std::string::npos;
}

}  // namespace

std::vector<Diagnostic> DiagnosticRunner::ParseCompilerOutput(const std::string& output, const std::string& target_file) {
    std::vector<Diagnostic> diagnostics;
    std::istringstream stream(output);
    std::string line;

    // GCC / Clang / Rustc: file:line:col: (error|warning|fatal error|note): message (supports Windows C:\ paths)
    static const std::regex kGccRegex(R"(^(.*?):(\d+):(\d+):\s+(fatal error|error|warning|note):\s+(.*)$)");

    // MSVC: file(line): error Cxxxx: message OR file(line,col): error Cxxxx: message (supports Windows C:\ paths)
    static const std::regex kMsvcRegex(R"(^(.*?)\s*\((\d+)(?:,(\d+))?\):\s+(fatal error|error|warning)\s+([A-Za-z0-9]+):\s+(.*)$)");

    // Python py_compile: File "path", line X
    static const std::regex kPyFileRegex("^\\s*File\\s+\"([^\"]+)\",\\s+line\\s+(\\d+)");
    static const std::regex kPyErrorRegex("^([A-Za-z0-9_]+Error):\\s*(.*)$");

    int py_pending_line = 0;
    std::string py_pending_file;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Ignore spurious GCC warning when checking .hpp/.h files directly
        if (line.find("#pragma once in main file") != std::string::npos ||
            line.find("-Wpragma-once-outside-header") != std::string::npos) {
            continue;
        }

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

    std::string file_dir = platform::GetDirectory(file_path);
    std::string cwd = working_dir.empty() ? file_dir : working_dir;

    std::thread([norm_path, ext, file_dir, cwd]() {
        std::vector<Diagnostic> diags;

        if (ext == ".py" || ext == ".pyw") {
            std::string cmd = "python -m py_compile \"" + norm_path + "\"";
            auto res = platform::RunCommand(cmd, cwd);
            diags = ParseCompilerOutput(res.output, norm_path);
        } else if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c" ||
                   ext == ".hpp" || ext == ".h") {
            bool is_header = (ext == ".hpp" || ext == ".h");
            std::vector<std::string> candidates = {"g++", "clang++", "cl.exe"};

            for (const auto& comp : candidates) {
                std::string cmd;
                if (comp == "cl.exe" || comp == "cl") {
                    cmd = "cl.exe /Zs /std:c++20 /nologo /I\"" + cwd + "\" /I\"" + file_dir + "\" \"" + norm_path + "\"";
                } else {
                    std::string header_flag = is_header ? "-x c++-header -Wno-pragma-once-outside-header " : "";
                    cmd = comp + " -fsyntax-only -Wall -Wno-pragma-once-outside-header -std=c++20 " + header_flag +
                          "-I\"" + cwd + "\" -I\"" + file_dir + "\" \"" + norm_path + "\"";
                }

                auto res = platform::RunCommand(cmd, cwd);
                if (IsCompilerMissing(res.output)) {
                    continue; // Try next candidate compiler
                }

                diags = ParseCompilerOutput(res.output, norm_path);
                break;
            }
        } else if (ext == ".rs") {
            std::string cmd = "rustc --error-format=short \"" + norm_path + "\"";
            auto res = platform::RunCommand(cmd, cwd);
            diags = ParseCompilerOutput(res.output, norm_path);
        }

        // Fix up relative file paths if compiler printed relative names, and record origin_file
        for (auto& d : diags) {
            d.origin_file = norm_path;
            std::error_code ec;
            if (!fs::path(d.file_path).is_absolute() && !norm_path.empty()) {
                if (NormalizePath(platform::GetFilename(d.file_path)) == NormalizePath(platform::GetFilename(norm_path))) {
                    d.file_path = norm_path;
                } else if (!file_dir.empty()) {
                    auto full = fs::path(file_dir) / d.file_path;
                    d.file_path = NormalizePath(full.lexically_normal().generic_string());
                }
            } else {
                d.file_path = NormalizePath(d.file_path);
            }
        }

        DiagnosticManager::Instance().SetDiagnosticsForFile(norm_path, diags);
    }).detach();
}

}  // namespace luce
