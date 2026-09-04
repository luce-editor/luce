#pragma once
// ============================================================================
// TerminalPanel — Embedded interactive terminal (shell subprocess).
// ============================================================================

#include "platform.h"
#include "ui/theme.h"

#include <memory>
#include <string>
#include <vector>

namespace luce {

class TerminalPanel {
public:
    TerminalPanel();
    ~TerminalPanel();

    /// Start the default shell in a new terminal tab.
    void StartShell();

    /// Create a brand-new terminal tab.
    void NewTerminal();

    /// Close the active terminal tab.
    void CloseCurrentTerminal();

    /// Check if the active terminal subprocess is running.
    bool IsRunning() const;

    /// Render the terminal panel.
    void Render(const Theme& theme);

    void SetWorkingDirectory(const std::string& cwd) { working_dir_ = cwd; }
    const std::string& GetWorkingDirectory() const { return working_dir_; }

    bool visible = true;

private:
    struct TerminalSession {
        uint64_t          id = 0;
        platform::Process process;
        std::string       title;
        void*             vterm = nullptr; // VTerm*
        void*             vterm_screen = nullptr; // VTermScreen*
        int               cols = 80;
        int               rows = 24;
        bool              needs_scroll_to_bottom = false;
        bool              shell_started = false;
        
        static void OutputCallback(const char* s, size_t len, void* user);

        ~TerminalSession(); // we need custom destructor to free vterm
        TerminalSession();
        TerminalSession(const TerminalSession&) = delete;
        TerminalSession& operator=(const TerminalSession&) = delete;
        TerminalSession(TerminalSession&&) = delete;
        TerminalSession& operator=(TerminalSession&&) = delete;
    };

    /// Read any available output from all running terminal sessions.
    void PollOutput();

    /// Create a shell session with the default OS shell.
    void StartShellInSession(TerminalSession& session);

    void EnsureShellStarted(TerminalSession& session);

    std::vector<std::unique_ptr<TerminalSession>> sessions_;
    size_t active_session_index_ = 0;
    bool request_select_tab_ = false;
    uint64_t next_session_id_ = 1;
    std::string working_dir_;
};

}  // namespace luce
