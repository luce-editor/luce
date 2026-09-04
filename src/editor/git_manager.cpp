#include "git_manager.h"
#include "platform.h"

#include <algorithm>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace luce {

namespace {

std::string Trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string NormalizePath(std::string p) {
    std::ranges::replace(p, '\\', '/');
    return p;
}

}  // namespace

void GitManager::SetRepoPath(const std::string& path) {
    repo_path_ = NormalizePath(path);
    Refresh();
}

char GitManager::GetFileStatusCode(const std::string& rel_or_abs_path) const {
    if (!has_repo_) return '\0';

    std::string norm = NormalizePath(rel_or_abs_path);

    // If it's an absolute path, try stripping repo_path_
    if (!repo_path_.empty() && norm.starts_with(repo_path_)) {
        norm = norm.substr(repo_path_.length());
        if (!norm.empty() && norm[0] == '/') norm = norm.substr(1);
    }

    auto it = file_status_map_.find(norm);
    if (it != file_status_map_.end()) return it->second;

    return '\0';
}

void GitManager::Refresh() {
    staged_changes_.clear();
    unstaged_changes_.clear();
    file_status_map_.clear();

    if (repo_path_.empty()) {
        has_repo_ = false;
        branch_.clear();
        return;
    }

    // Check if repo exists
    auto res_check = platform::RunCommand("git rev-parse --is-inside-work-tree", repo_path_);
    if (res_check.exit_code != 0 || Trim(res_check.output) != "true") {
        has_repo_ = false;
        branch_.clear();
        return;
    }

    has_repo_ = true;

    // Get current branch
    auto res_branch = platform::RunCommand("git branch --show-current", repo_path_);
    std::string b = Trim(res_branch.output);
    if (b.empty()) {
        auto res_head = platform::RunCommand("git rev-parse --short HEAD", repo_path_);
        b = Trim(res_head.output);
        if (b.empty()) b = "HEAD (detached)";
    }
    branch_ = b;

    // Get porcelain status
    auto res_status = platform::RunCommand("git status --porcelain=v1 -uall", repo_path_);
    if (res_status.exit_code != 0) return;

    std::istringstream stream(res_status.output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.length() < 4) continue;

        char x = line[0];
        char y = line[1];
        std::string raw_path = Trim(line.substr(3));

        // Strip quotes if git quotes filenames with spaces
        if (raw_path.length() >= 2 && raw_path.front() == '"' && raw_path.back() == '"') {
            raw_path = raw_path.substr(1, raw_path.length() - 2);
        }
        std::string file_path = NormalizePath(raw_path);

        // Staged status (index)
        if (x != ' ' && x != '?') {
            GitFileStatus st;
            st.path = file_path;
            st.is_staged = true;

            if (x == 'A') st.type = GitStatusType::Added;
            else if (x == 'D') st.type = GitStatusType::Deleted;
            else if (x == 'R') st.type = GitStatusType::Renamed;
            else st.type = GitStatusType::Modified;

            staged_changes_.push_back(st);
            file_status_map_[file_path] = (x == 'A' ? 'A' : (x == 'D' ? 'D' : 'M'));
        }

        // Unstaged status (working tree)
        if (y != ' ') {
            GitFileStatus st;
            st.path = file_path;
            st.is_staged = false;

            if (x == '?' && y == '?') {
                st.type = GitStatusType::Untracked;
                file_status_map_[file_path] = 'U';
            } else if (y == 'D') {
                st.type = GitStatusType::Deleted;
                file_status_map_[file_path] = 'D';
            } else {
                st.type = GitStatusType::Modified;
                file_status_map_[file_path] = 'M';
            }

            unstaged_changes_.push_back(st);
        }
    }
}

bool GitManager::StageFile(const std::string& rel_path) {
    if (!has_repo_) return false;
    auto res = platform::RunCommand("git add \"" + rel_path + "\"", repo_path_);
    Refresh();
    return res.exit_code == 0;
}

bool GitManager::UnstageFile(const std::string& rel_path) {
    if (!has_repo_) return false;
    auto res = platform::RunCommand("git restore --staged \"" + rel_path + "\"", repo_path_);
    if (res.exit_code != 0) {
        // Fallback for older git
        res = platform::RunCommand("git reset HEAD \"" + rel_path + "\"", repo_path_);
    }
    Refresh();
    return res.exit_code == 0;
}

bool GitManager::StageAll() {
    if (!has_repo_) return false;
    auto res = platform::RunCommand("git add -A", repo_path_);
    Refresh();
    return res.exit_code == 0;
}

bool GitManager::UnstageAll() {
    if (!has_repo_) return false;
    auto res = platform::RunCommand("git restore --staged .", repo_path_);
    if (res.exit_code != 0) {
        res = platform::RunCommand("git reset HEAD", repo_path_);
    }
    Refresh();
    return res.exit_code == 0;
}

bool GitManager::DiscardChanges(const std::string& rel_path) {
    if (!has_repo_) return false;
    auto res = platform::RunCommand("git restore \"" + rel_path + "\"", repo_path_);
    if (res.exit_code != 0) {
        // If untracked file
        res = platform::RunCommand("git clean -f \"" + rel_path + "\"", repo_path_);
    }
    Refresh();
    return res.exit_code == 0;
}

bool GitManager::Commit(const std::string& message) {
    if (!has_repo_) return false;
    std::string msg = Trim(message);
    if (msg.empty()) return false;

    // Escape quotes for cmd
    std::string escaped;
    for (char c : msg) {
        if (c == '"') escaped += "\\\"";
        else escaped += c;
    }

    auto res = platform::RunCommand("git commit -m \"" + escaped + "\"", repo_path_);
    Refresh();
    return res.exit_code == 0;
}

}  // namespace luce
