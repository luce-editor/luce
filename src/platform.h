#pragma once
// ============================================================================
// Platform — OS abstraction for file dialogs, dynamic loading, and processes.
//
// This module wraps platform-specific APIs behind a portable C++ interface.
// Currently targets Windows; Linux/macOS stubs can be added without touching
// the rest of the codebase.
// ============================================================================

#include <functional>
#include <string>
#include <vector>

namespace luce::platform {

// ── File dialogs ──────────────────────────────────────────────────────────

/// Show a native "Open File" dialog.  Returns the chosen path, or "" if
/// the user cancelled.
std::string OpenFileDialog(const char* filter = "All Files\0*.*\0");

/// Show a native "Save File" dialog.
std::string SaveFileDialog(const char* filter = "All Files\0*.*\0");

/// Show a native "Open Folder" dialog.
std::string OpenFolderDialog();

// ── Filesystem helpers ────────────────────────────────────────────────────

/// Return the directory part of a path (e.g. "C:/foo/bar.txt" → "C:/foo").
std::string GetDirectory(const std::string& path);

/// Return the filename part of a path (e.g. "C:/foo/bar.txt" → "bar.txt").
std::string GetFilename(const std::string& path);

/// Return the file extension including the dot (e.g. ".cpp").
std::string GetExtension(const std::string& path);

/// Return the path to the running executable.
std::string GetExecutableDir();

/// Open a directory or file in the native system file explorer.
void OpenInFileExplorer(const std::string& path);

// ── Dynamic library loading (used by future plugin system) ────────────────

using LibraryHandle = void*;

LibraryHandle LoadDynamicLibrary(const char* path);
void          FreeDynamicLibrary(LibraryHandle handle);
void*         GetSymbol(LibraryHandle handle, const char* name);

// ── Synchronous command execution ─────────────────────────────────────────

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

/// Execute a shell command silently (no console window popup) and capture its output.
CommandResult RunCommand(const std::string& command, const std::string& cwd = "");

// ── Process spawning (used by the embedded terminal) ──────────────────────

/// An interactive subprocess with piped stdin / stdout / stderr.
/// The caller writes commands via `Write()` and reads output via `Read()`.
class Process {
public:
    Process();
    ~Process();

    /// Spawn a new process.  `command` is the program to run (e.g. "cmd.exe"
    /// or "powershell.exe"). `cols` and `rows` specify the initial pseudo-console size.
    /// Returns true on success.
    bool Start(const std::string& command, int cols = 80, int rows = 24, const std::string& cwd = "");

    /// Write raw bytes to the process's stdin.
    bool Write(const std::string& data);

    /// Read available bytes from the process's stdout+stderr.
    /// Non-blocking: returns an empty string if nothing is available.
    std::string Read();

    /// Check whether the process is still running.
    bool IsRunning() const;

    /// Resize the pseudo-console.
    void Resize(int cols, int rows);

    /// Forcefully terminate the process.
    void Kill();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace luce::platform
