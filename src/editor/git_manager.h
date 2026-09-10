#pragma once
// ============================================================================
// GitManager — Git source control integration for Luce.
//
// Detects Git repository, tracks branch name, lists staged and unstaged
// changes, parses 'git status --porcelain', and provides actions to stage,
// unstage, discard, and commit files.
// ============================================================================

#include "platform.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

namespace luce {

enum class GitStatusType {
    Modified,    // 'M'
    Untracked,   // '?'
    Added,       // 'A'
    Deleted,     // 'D'
    Renamed,     // 'R'
    Unknown
};

enum class GitLineDiffType {
    None,
    Added,
    Modified,
    Deleted
};

struct GitFileDiffMarks {
    std::unordered_map<int, GitLineDiffType> lines; // 0-indexed line number -> diff type
};

struct GitFileStatus {
    std::string path;
    GitStatusType type = GitStatusType::Modified;
    bool is_staged = false;
};

struct GitLogEntry {
    std::string timestamp;
    std::string command;
    int exit_code = 0;
    std::string output;
};

struct GitStashEntry {
    int index = 0;
    std::string name;
    std::string message;
};

class GitManager {
public:
    static GitManager& Instance() {
        static GitManager instance;
        return instance;
    }

    ~GitManager();

    void SetRepoPath(const std::string& path);
    const std::string& GetRepoPath() const { return repo_path_; }

    bool HasRepo() const {
        ApplyPendingUpdates();
        return has_repo_;
    }
    const std::string& GetBranch() const {
        ApplyPendingUpdates();
        return branch_;
    }

    const std::vector<GitFileStatus>& GetStagedChanges() const {
        ApplyPendingUpdates();
        return staged_changes_;
    }
    const std::vector<GitFileStatus>& GetUnstagedChanges() const {
        ApplyPendingUpdates();
        return unstaged_changes_;
    }

    /// Returns status code for file tree badge ('M', 'U', 'D', 'A', or '\0')
    char GetFileStatusCode(const std::string& rel_or_abs_path) const;

    /// Returns line-by-line diff markers for editor gutter (0-indexed lines)
    GitFileDiffMarks GetFileDiffMarks(const std::string& rel_or_abs_path) const;

    /// Returns full unified diff text for a file
    std::string GetFileDiff(const std::string& rel_or_abs_path) const;

    void Refresh();
    void RefreshAsync();
    void RefreshStatusOnly();
    bool StageFile(const std::string& rel_path);
    bool UnstageFile(const std::string& rel_path);
    bool StageAll();
    bool UnstageAll();
    bool DiscardChanges(const std::string& rel_path);
    bool DiscardAll(std::string& out_error);
    bool Commit(const std::string& message, std::string& out_error);
    bool Commit(const std::string& message) {
        std::string err;
        return Commit(message, err);
    }
    bool CommitAmend(const std::string& new_message, std::string& out_error);
    bool CommitAll(const std::string& message, std::string& out_error);
    bool UndoLastCommit(bool keep_staged, std::string& out_error);

    /// Branch management
    const std::vector<std::string>& GetBranchList() const;
    bool CheckoutBranch(const std::string& branch_name, std::string& out_error);
    bool CreateBranch(const std::string& branch_name, bool checkout, std::string& out_error);
    bool DeleteBranch(const std::string& branch_name, bool force, std::string& out_error);
    bool RenameBranch(const std::string& old_name, const std::string& new_name, std::string& out_error);
    bool MergeBranch(const std::string& branch_name, std::string& out_error);

    /// Remote & Sync
    bool HasRemote(const std::string& remote_name = "origin") const;
    std::string GetRemoteUrl(const std::string& remote_name = "origin");
    std::vector<std::string> GetRemoteList() const;
    bool AddRemote(const std::string& remote_name, const std::string& url, std::string& out_error);
    bool RemoveRemote(const std::string& remote_name, std::string& out_error);
    bool Push(bool set_upstream, std::string& out_error);
    bool PushForce(std::string& out_error);
    bool Pull(std::string& out_error);
    bool Fetch(std::string& out_error);
    bool FetchPrune(std::string& out_error);

    /// Stash management
    std::vector<GitStashEntry> GetStashList() const;
    bool StashSave(const std::string& message, bool include_untracked, bool keep_index, std::string& out_error);
    bool StashPop(int index, std::string& out_error);
    bool StashApply(int index, std::string& out_error);
    bool StashDrop(int index, std::string& out_error);

    /// Tags management
    std::vector<std::string> GetTagList() const;
    bool CreateTag(const std::string& tag_name, const std::string& message, std::string& out_error);
    bool DeleteTag(const std::string& tag_name, std::string& out_error);
    bool PushTags(std::string& out_error);

    /// Clone
    static bool CloneRepo(const std::string& url, const std::string& target_path, std::string& out_error);

    /// Command logging
    std::vector<GitLogEntry> GetCommandLog() const;
    void ClearCommandLog();

    int GetAheadCount() const {
        ApplyPendingUpdates();
        return ahead_count_;
    }
    int GetBehindCount() const {
        ApplyPendingUpdates();
        return behind_count_;
    }

    void WaitForPendingTasks();

private:
    struct GitState {
        bool has_repo = false;
        std::string branch;
        int ahead_count = 0;
        int behind_count = 0;
        bool has_remote_origin = false;
        std::vector<std::string> branch_list;
        std::vector<GitFileStatus> staged;
        std::vector<GitFileStatus> unstaged;
        std::unordered_map<std::string, char> file_map;
    };

    GitManager();
    void ApplyPendingUpdates() const;
    void EnqueueTask(std::function<void()> task);
    platform::CommandResult RunGit(const std::string& args) const;
    GitState QueryGitState() const;

    std::string repo_path_;
    mutable bool has_repo_ = false;
    mutable std::string branch_;
    mutable int ahead_count_ = 0;
    mutable int behind_count_ = 0;
    mutable bool has_remote_origin_ = false;
    mutable std::vector<std::string> branch_list_;
    mutable std::vector<GitFileStatus> staged_changes_;
    mutable std::vector<GitFileStatus> unstaged_changes_;
    mutable std::unordered_map<std::string, char> file_status_map_;

    // Background worker thread for non-blocking Git operations
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable tasks_done_cv_;
    std::atomic<int> pending_tasks_{0};
    std::thread worker_thread_;
    std::atomic<bool> stop_worker_{false};

    mutable std::mutex pending_mutex_;
    mutable std::atomic<bool> has_pending_status_{false};
    mutable GitState pending_state_;

    mutable std::mutex log_mutex_;
    mutable std::vector<GitLogEntry> command_log_;
};

}  // namespace luce
