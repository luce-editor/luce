#pragma once
// ============================================================================
// GitManager — Git source control integration for Luce.
//
// Detects Git repository, tracks branch name, lists staged and unstaged
// changes, parses 'git status --porcelain', and provides actions to stage,
// unstage, discard, and commit files.
// ============================================================================

#include <string>
#include <vector>
#include <unordered_map>

namespace luce {

enum class GitStatusType {
    Modified,    // 'M'
    Untracked,   // '?'
    Added,       // 'A'
    Deleted,     // 'D'
    Renamed,     // 'R'
    Unknown
};

struct GitFileStatus {
    std::string path;
    GitStatusType type = GitStatusType::Modified;
    bool is_staged = false;
};

class GitManager {
public:
    static GitManager& Instance() {
        static GitManager instance;
        return instance;
    }

    void SetRepoPath(const std::string& path);
    const std::string& GetRepoPath() const { return repo_path_; }

    bool HasRepo() const { return has_repo_; }
    const std::string& GetBranch() const { return branch_; }

    const std::vector<GitFileStatus>& GetStagedChanges() const { return staged_changes_; }
    const std::vector<GitFileStatus>& GetUnstagedChanges() const { return unstaged_changes_; }

    /// Returns status code for file tree badge ('M', 'U', 'D', 'A', or '\0')
    char GetFileStatusCode(const std::string& rel_or_abs_path) const;

    void Refresh();
    bool StageFile(const std::string& rel_path);
    bool UnstageFile(const std::string& rel_path);
    bool StageAll();
    bool UnstageAll();
    bool DiscardChanges(const std::string& rel_path);
    bool Commit(const std::string& message);

private:
    GitManager() = default;

    std::string repo_path_;
    bool has_repo_ = false;
    std::string branch_;
    std::vector<GitFileStatus> staged_changes_;
    std::vector<GitFileStatus> unstaged_changes_;
    std::unordered_map<std::string, char> file_status_map_;
};

}  // namespace luce
