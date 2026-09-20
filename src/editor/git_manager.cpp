#include "git_manager.h"
#include "platform.h"

#include <algorithm>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <ctime>

namespace fs = std::filesystem;

namespace luce {

namespace {

std::string GetCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
#if defined(_WIN32)
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    std::ostringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S");
    return ss.str();
}

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

void ParsePorcelain(const std::string& output,
                    std::vector<GitFileStatus>& staged,
                    std::vector<GitFileStatus>& unstaged,
                    std::unordered_map<std::string, char>& status_map) {
    staged.clear();
    unstaged.clear();
    status_map.clear();

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.length() < 4) continue;

        char x = line[0];
        char y = line[1];
        std::string raw_path = Trim(line.substr(3));

        if (raw_path.length() >= 2 && raw_path.front() == '"' && raw_path.back() == '"') {
            raw_path = raw_path.substr(1, raw_path.length() - 2);
        }
        std::string file_path = NormalizePath(raw_path);

        if (x != ' ' && x != '?') {
            GitFileStatus st;
            st.path = file_path;
            st.is_staged = true;

            if (x == 'A') st.type = GitStatusType::Added;
            else if (x == 'D') st.type = GitStatusType::Deleted;
            else if (x == 'R') st.type = GitStatusType::Renamed;
            else st.type = GitStatusType::Modified;

            staged.push_back(st);
            status_map[file_path] = (x == 'A' ? 'A' : (x == 'D' ? 'D' : 'M'));
        }

        if (y != ' ') {
            GitFileStatus st;
            st.path = file_path;
            st.is_staged = false;

            if (x == '?' && y == '?') {
                st.type = GitStatusType::Untracked;
                status_map[file_path] = 'U';
            } else if (y == 'D') {
                st.type = GitStatusType::Deleted;
                status_map[file_path] = 'D';
            } else {
                st.type = GitStatusType::Modified;
                status_map[file_path] = 'M';
            }

            unstaged.push_back(st);
        }
    }
}

}  // namespace

GitManager::GitManager() {
    worker_thread_ = std::thread([this]() {
        while (!stop_worker_) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() {
                    return stop_worker_.load() || !task_queue_.empty();
                });
                if (stop_worker_ && task_queue_.empty()) break;
                if (!task_queue_.empty()) {
                    task = std::move(task_queue_.front());
                    task_queue_.pop();
                }
            }
            if (task) {
                task();
                --pending_tasks_;
                tasks_done_cv_.notify_all();
            }
        }
    });
}

GitManager::~GitManager() {
    stop_worker_ = true;
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void GitManager::EnqueueTask(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        ++pending_tasks_;
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void GitManager::WaitForPendingTasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    tasks_done_cv_.wait(lock, [this]() {
        return pending_tasks_.load() == 0 && task_queue_.empty();
    });
}

void GitManager::ApplyPendingUpdates() const {
    if (!has_pending_status_.load()) return;
    std::lock_guard<std::mutex> lock(pending_mutex_);
    if (has_pending_status_.load()) {
        has_repo_ = pending_state_.has_repo;
        if (!has_repo_) {
            branch_.clear();
            ahead_count_ = 0;
            behind_count_ = 0;
            has_remote_origin_ = false;
            branch_list_.clear();
            staged_changes_.clear();
            unstaged_changes_.clear();
            file_status_map_.clear();
        } else {
            if (!pending_state_.branch.empty()) {
                branch_ = std::move(pending_state_.branch);
            }
            ahead_count_ = pending_state_.ahead_count;
            behind_count_ = pending_state_.behind_count;
            has_remote_origin_ = pending_state_.has_remote_origin;
            if (!pending_state_.branch_list.empty()) {
                branch_list_ = std::move(pending_state_.branch_list);
            }
            staged_changes_ = std::move(pending_state_.staged);
            unstaged_changes_ = std::move(pending_state_.unstaged);
            file_status_map_ = std::move(pending_state_.file_map);
        }
        has_pending_status_.store(false);
    }
}

platform::CommandResult GitManager::RunGit(const std::string& args) const {
    if (repo_path_.empty()) return {};
    std::string win_path = repo_path_;
    std::ranges::replace(win_path, '/', '\\');
    while (!win_path.empty() && win_path.back() == '\\') {
        win_path.pop_back();
    }
    auto res = platform::RunCommand("git " + args, win_path);

    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        GitLogEntry entry;
        entry.timestamp = GetCurrentTimeString();
        entry.command = "git " + args;
        entry.exit_code = res.exit_code;
        entry.output = res.output;
        command_log_.push_back(std::move(entry));
        if (command_log_.size() > 100) {
            command_log_.erase(command_log_.begin());
        }
    }

    return res;
}

std::vector<GitLogEntry> GitManager::GetCommandLog() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return command_log_;
}

void GitManager::ClearCommandLog() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    command_log_.clear();
}

void GitManager::SetRepoPath(const std::string& path) {
    repo_path_ = NormalizePath(path);
    RefreshAsync();
}

char GitManager::GetFileStatusCode(const std::string& rel_or_abs_path) const {
    ApplyPendingUpdates();
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

GitFileDiffMarks GitManager::GetFileDiffMarks(const std::string& rel_or_abs_path) const {
    GitFileDiffMarks marks;
    if (!has_repo_ || repo_path_.empty()) return marks;

    std::string norm = NormalizePath(rel_or_abs_path);
    std::string repo = repo_path_;

    bool is_inside = false;
#if defined(_WIN32)
    std::string lower_norm = norm;
    std::string lower_repo = repo;
    std::ranges::transform(lower_norm, lower_norm.begin(), ::tolower);
    std::ranges::transform(lower_repo, lower_repo.begin(), ::tolower);
    if (lower_norm.starts_with(lower_repo)) {
        norm = norm.substr(repo.length());
        is_inside = true;
    }
#else
    if (norm.starts_with(repo)) {
        norm = norm.substr(repo.length());
        is_inside = true;
    }
#endif

    // If file is an absolute path outside the repo, skip git diff completely
    if (!is_inside && !norm.empty() && (norm[0] == '/' || (norm.size() > 2 && norm[1] == ':'))) {
        return marks;
    }

    if (!norm.empty() && norm[0] == '/') norm = norm.substr(1);
    if (norm.empty()) return marks;

    auto res = RunGit("diff HEAD -U0 -- \"" + norm + "\"");
    if (res.exit_code != 0 || res.output.empty()) {
        res = RunGit("diff -U0 -- \"" + norm + "\"");
    }

    if (res.output.empty()) {
        return marks;
    }

    std::istringstream iss(res.output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.starts_with("@@")) continue;
        size_t plus_pos = line.find('+');
        size_t minus_pos = line.find('-');
        if (plus_pos == std::string::npos || minus_pos == std::string::npos) continue;

        int old_start = 0, old_count = 1;
        int new_start = 0, new_count = 1;

        std::string old_part = line.substr(minus_pos + 1, plus_pos - (minus_pos + 1));
        size_t comma = old_part.find(',');
        if (comma != std::string::npos) {
            old_start = std::atoi(old_part.substr(0, comma).c_str());
            old_count = std::atoi(old_part.substr(comma + 1).c_str());
        } else {
            old_start = std::atoi(old_part.c_str());
        }

        size_t end_hunk = line.find("@@", plus_pos);
        if (end_hunk == std::string::npos) continue;
        std::string new_part = line.substr(plus_pos + 1, end_hunk - (plus_pos + 1));
        while (!new_part.empty() && new_part.back() == ' ') new_part.pop_back();
        comma = new_part.find(',');
        if (comma != std::string::npos) {
            new_start = std::atoi(new_part.substr(0, comma).c_str());
            new_count = std::atoi(new_part.substr(comma + 1).c_str());
        } else {
            new_start = std::atoi(new_part.c_str());
        }

        if (new_count == 0) {
            int target_line = (new_start > 0) ? (new_start - 1) : 0;
            marks.lines[target_line] = GitLineDiffType::Deleted;
        } else if (old_count == 0) {
            for (int i = 0; i < new_count; ++i) {
                int target_line = new_start - 1 + i;
                if (target_line >= 0) {
                    marks.lines[target_line] = GitLineDiffType::Added;
                }
            }
        } else {
            for (int i = 0; i < new_count; ++i) {
                int target_line = new_start - 1 + i;
                if (target_line >= 0) {
                    marks.lines[target_line] = GitLineDiffType::Modified;
                }
            }
        }
    }

    return marks;
}

std::string GitManager::GetFileDiff(const std::string& rel_or_abs_path) const {
    if (repo_path_.empty()) return "";

    std::string norm = NormalizePath(rel_or_abs_path);
    if (!repo_path_.empty() && norm.starts_with(repo_path_)) {
        norm = norm.substr(repo_path_.length());
        if (!norm.empty() && norm[0] == '/') norm = norm.substr(1);
    }
    if (norm.empty()) return "";

    auto res = RunGit("diff HEAD -u -- \"" + norm + "\"");
    if (res.exit_code != 0 || res.output.empty()) {
        res = RunGit("diff -u -- \"" + norm + "\"");
    }

    if (res.output.empty()) {
        char st = GetFileStatusCode(norm);
        if (st == '?') {
            return "--- /dev/null\n+++ b/" + norm + "\n@@ -0,0 +1 @@\n+ (Untracked file: entire file is new)";
        }
        return "No changes detected.";
    }

    return res.output;
}

GitManager::GitState GitManager::QueryGitState() const {
    GitState state;
    if (repo_path_.empty()) return state;

    // Check if repo exists
    auto res_check = RunGit("rev-parse --is-inside-work-tree");
    if (res_check.exit_code != 0 || Trim(res_check.output) != "true") {
        return state;
    }
    state.has_repo = true;

    // Get current branch
    auto res_branch = RunGit("branch --show-current");
    std::string b = Trim(res_branch.output);
    if (b.empty()) {
        auto res_head = RunGit("rev-parse --short HEAD");
        b = Trim(res_head.output);
        if (b.empty()) b = "HEAD (detached)";
    }
    state.branch = b;

    // Calculate ahead/behind counts if upstream exists
    auto res_counts = RunGit("rev-list --left-right --count HEAD...@{u}");
    if (res_counts.exit_code == 0) {
        std::istringstream iss(res_counts.output);
        int a = 0, b_cnt = 0;
        if (iss >> a >> b_cnt) {
            state.ahead_count = a;
            state.behind_count = b_cnt;
        }
    }

    // Branch list
    auto res_branches = RunGit("branch --list");
    if (res_branches.exit_code == 0) {
        std::istringstream stream(res_branches.output);
        std::string line;
        while (std::getline(stream, line)) {
            size_t start = line.find_first_not_of(" *+\t\r\n");
            if (start != std::string::npos) {
                std::string branch_name = Trim(line.substr(start));
                if (!branch_name.empty() && !branch_name.starts_with("(HEAD detached")) {
                    state.branch_list.push_back(branch_name);
                }
            }
        }
    }

    // Check remote origin
    auto res_remote = RunGit("remote");
    if (res_remote.exit_code == 0) {
        std::istringstream stream(res_remote.output);
        std::string line;
        while (std::getline(stream, line)) {
            if (Trim(line) == "origin") {
                state.has_remote_origin = true;
                break;
            }
        }
    }

    // Get porcelain status
    auto res_status = RunGit("status --porcelain=v1 -uall");
    if (res_status.exit_code == 0) {
        ParsePorcelain(res_status.output, state.staged, state.unstaged, state.file_map);
    }

    return state;
}

void GitManager::Refresh() {
    WaitForPendingTasks();
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        has_pending_status_.store(false);
    }

    auto state = QueryGitState();
    has_repo_ = state.has_repo;
    branch_ = std::move(state.branch);
    ahead_count_ = state.ahead_count;
    behind_count_ = state.behind_count;
    has_remote_origin_ = state.has_remote_origin;
    branch_list_ = std::move(state.branch_list);
    staged_changes_ = std::move(state.staged);
    unstaged_changes_ = std::move(state.unstaged);
    file_status_map_ = std::move(state.file_map);
}

void GitManager::RefreshAsync() {
    if (repo_path_.empty()) return;
    EnqueueTask([this]() {
        auto state = QueryGitState();
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_state_ = std::move(state);
        has_pending_status_.store(true);
    });
}

void GitManager::RefreshStatusOnly() {
    if (!has_repo_ || repo_path_.empty()) return;

    auto res_status = RunGit("status --porcelain=v1 -uall");
    if (res_status.exit_code == 0) {
        std::vector<GitFileStatus> staged;
        std::vector<GitFileStatus> unstaged;
        std::unordered_map<std::string, char> file_map;
        ParsePorcelain(res_status.output, staged, unstaged, file_map);

        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_state_.has_repo = true;
        pending_state_.staged = std::move(staged);
        pending_state_.unstaged = std::move(unstaged);
        pending_state_.file_map = std::move(file_map);
        has_pending_status_.store(true);
    }
}

bool GitManager::StageFile(const std::string& rel_path) {
    if (!has_repo_) return false;

    ApplyPendingUpdates();

    // Optimistic UI update: move item from unstaged to staged immediately
    for (auto it = unstaged_changes_.begin(); it != unstaged_changes_.end(); ++it) {
        if (it->path == rel_path) {
            GitFileStatus st = *it;
            st.is_staged = true;
            staged_changes_.push_back(st);
            unstaged_changes_.erase(it);
            break;
        }
    }

    EnqueueTask([this, rel_path]() {
        RunGit("add -- \"" + rel_path + "\"");
        RefreshStatusOnly();
    });

    return true;
}

bool GitManager::UnstageFile(const std::string& rel_path) {
    if (!has_repo_) return false;

    ApplyPendingUpdates();

    // Optimistic UI update: move item from staged to unstaged immediately
    for (auto it = staged_changes_.begin(); it != staged_changes_.end(); ++it) {
        if (it->path == rel_path) {
            GitFileStatus st = *it;
            st.is_staged = false;
            unstaged_changes_.push_back(st);
            staged_changes_.erase(it);
            break;
        }
    }

    EnqueueTask([this, rel_path]() {
        auto res = RunGit("restore --staged -- \"" + rel_path + "\"");
        if (res.exit_code != 0) {
            RunGit("reset HEAD -- \"" + rel_path + "\"");
        }
        RefreshStatusOnly();
    });

    return true;
}

bool GitManager::StageAll() {
    if (!has_repo_) return false;

    ApplyPendingUpdates();

    // Optimistic UI update: move all unstaged items to staged
    for (auto& item : unstaged_changes_) {
        item.is_staged = true;
        staged_changes_.push_back(item);
    }
    unstaged_changes_.clear();

    EnqueueTask([this]() {
        RunGit("add -A");
        RefreshStatusOnly();
    });

    return true;
}

bool GitManager::UnstageAll() {
    if (!has_repo_) return false;

    ApplyPendingUpdates();

    // Optimistic UI update: move all staged items to unstaged
    for (auto& item : staged_changes_) {
        item.is_staged = false;
        unstaged_changes_.push_back(item);
    }
    staged_changes_.clear();

    EnqueueTask([this]() {
        auto res = RunGit("restore --staged .");
        if (res.exit_code != 0) {
            RunGit("reset HEAD");
        }
        RefreshStatusOnly();
    });

    return true;
}

bool GitManager::DiscardChanges(const std::string& rel_path) {
    if (!has_repo_) return false;

    ApplyPendingUpdates();

    // Optimistic UI update: remove from unstaged list
    std::erase_if(unstaged_changes_, [&](const GitFileStatus& s) { return s.path == rel_path; });

    EnqueueTask([this, rel_path]() {
        auto res = RunGit("restore -- \"" + rel_path + "\"");
        if (res.exit_code != 0) {
            RunGit("clean -f -- \"" + rel_path + "\"");
        }
        RefreshStatusOnly();
    });

    return true;
}

bool GitManager::DiscardAll(std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    unstaged_changes_.clear();
    auto res = RunGit("restore .");
    RunGit("clean -fd");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to discard changes.";
        return false;
    }
    return true;
}

bool GitManager::Commit(const std::string& message, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string msg = Trim(message);
    if (msg.empty()) {
        out_error = "Commit message cannot be empty.";
        return false;
    }

    WaitForPendingTasks();
    ApplyPendingUpdates();

    if (staged_changes_.empty()) {
        out_error = "No staged changes to commit. Please stage changes first using '+' or 'Stage All Changes'.";
        return false;
    }

    // Escape quotes for cmd
    std::string escaped;
    for (char c : msg) {
        if (c == '"') escaped += "\\\"";
        else escaped += c;
    }

    auto res = RunGit("commit -m \"" + escaped + "\"");
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git commit failed.";
        return false;
    }
    staged_changes_.clear();
    RefreshAsync();
    return true;
}

bool GitManager::CommitAmend(const std::string& new_message, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    std::string cmd;
    std::string msg = Trim(new_message);
    if (msg.empty()) {
        cmd = "commit --amend --no-edit";
    } else {
        std::string escaped;
        for (char c : msg) {
            if (c == '"') escaped += "\\\"";
            else escaped += c;
        }
        cmd = "commit --amend -m \"" + escaped + "\"";
    }

    auto res = RunGit(cmd);
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git commit --amend failed.";
        return false;
    }
    staged_changes_.clear();
    RefreshAsync();
    return true;
}

bool GitManager::CommitAll(const std::string& message, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string msg = Trim(message);
    if (msg.empty()) {
        out_error = "Commit message cannot be empty.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    std::string escaped;
    for (char c : msg) {
        if (c == '"') escaped += "\\\"";
        else escaped += c;
    }

    auto res = RunGit("commit -a -m \"" + escaped + "\"");
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git commit -a failed.";
        return false;
    }
    staged_changes_.clear();
    unstaged_changes_.clear();
    RefreshAsync();
    return true;
}

bool GitManager::UndoLastCommit(bool keep_staged, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    std::string cmd = keep_staged ? "reset --soft HEAD~1" : "reset HEAD~1";
    auto res = RunGit(cmd);
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to undo last commit.";
        return false;
    }
    RefreshAsync();
    return true;
}

const std::vector<std::string>& GitManager::GetBranchList() const {
    ApplyPendingUpdates();
    return branch_list_;
}

bool GitManager::CheckoutBranch(const std::string& branch_name, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string b = Trim(branch_name);
    if (b.empty()) {
        out_error = "Branch name cannot be empty.";
        return false;
    }

    WaitForPendingTasks();
    ApplyPendingUpdates();

    auto res = RunGit("checkout \"" + b + "\"");
    if (res.exit_code != 0) {
        res = RunGit("switch \"" + b + "\"");
    }

    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to checkout branch: " + b;
        return false;
    }

    branch_ = b;
    RefreshAsync();
    return true;
}

bool GitManager::CreateBranch(const std::string& branch_name, bool checkout, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string b = Trim(branch_name);
    if (b.empty()) {
        out_error = "Branch name cannot be empty.";
        return false;
    }

    WaitForPendingTasks();
    ApplyPendingUpdates();

    std::string cmd = checkout ? ("checkout -b \"" + b + "\"") : ("branch \"" + b + "\"");
    auto res = RunGit(cmd);
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to create branch: " + b;
        return false;
    }

    if (checkout) {
        branch_ = b;
    }
    RefreshAsync();
    return true;
}

bool GitManager::HasRemote(const std::string& remote_name) const {
    ApplyPendingUpdates();
    if (!has_repo_) return false;
    if (remote_name == "origin") {
        return has_remote_origin_;
    }
    auto res = RunGit("remote");
    if (res.exit_code != 0) return false;

    std::istringstream stream(res.output);
    std::string line;
    while (std::getline(stream, line)) {
        if (Trim(line) == remote_name) return true;
    }
    return false;
}

std::string GitManager::GetRemoteUrl(const std::string& remote_name) {
    if (!has_repo_) return "";
    auto res = RunGit("remote get-url \"" + remote_name + "\"");
    if (res.exit_code == 0) {
        return Trim(res.output);
    }
    return "";
}

bool GitManager::AddRemote(const std::string& remote_name, const std::string& url, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string rname = Trim(remote_name);
    std::string rurl = Trim(url);
    if (rname.empty()) rname = "origin";
    if (rurl.empty()) {
        out_error = "Remote URL cannot be empty.";
        return false;
    }

    auto res = RunGit("remote add \"" + rname + "\" \"" + rurl + "\"");
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        return false;
    }
    return true;
}

bool GitManager::Push(bool set_upstream, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    if (branch_.empty() || branch_.starts_with("HEAD")) {
        out_error = "Cannot push in detached HEAD state.";
        return false;
    }

    std::string cmd;
    if (set_upstream) {
        cmd = "push -u origin \"" + branch_ + "\"";
    } else {
        cmd = "push";
    }

    auto res = RunGit(cmd);
    if (res.exit_code != 0) {
        if (res.output.find("no upstream branch") != std::string::npos ||
            res.output.find("set-upstream") != std::string::npos) {
            res = RunGit("push -u origin \"" + branch_ + "\"");
        }
    }

    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git push failed.";
        return false;
    }
    return true;
}

bool GitManager::Pull(std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }

    auto res = RunGit("pull");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git pull failed.";
        return false;
    }
    return true;
}

bool GitManager::Fetch(std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }

    auto res = RunGit("fetch");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git fetch failed.";
        return false;
    }
    return true;
}

bool GitManager::FetchPrune(std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }

    auto res = RunGit("fetch --prune");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git fetch --prune failed.";
        return false;
    }
    return true;
}

bool GitManager::DeleteBranch(const std::string& branch_name, bool force, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string b = Trim(branch_name);
    if (b.empty()) {
        out_error = "Branch name cannot be empty.";
        return false;
    }
    if (b == branch_) {
        out_error = "Cannot delete the currently active branch. Switch to another branch first.";
        return false;
    }

    WaitForPendingTasks();
    ApplyPendingUpdates();

    std::string cmd = force ? ("branch -D \"" + b + "\"") : ("branch -d \"" + b + "\"");
    auto res = RunGit(cmd);
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to delete branch: " + b;
        return false;
    }
    RefreshAsync();
    return true;
}

bool GitManager::RenameBranch(const std::string& old_name, const std::string& new_name, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string old_b = Trim(old_name);
    std::string new_b = Trim(new_name);
    if (old_b.empty() || new_b.empty()) {
        out_error = "Branch names cannot be empty.";
        return false;
    }

    WaitForPendingTasks();
    ApplyPendingUpdates();

    auto res = RunGit("branch -m \"" + old_b + "\" \"" + new_b + "\"");
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to rename branch.";
        return false;
    }
    if (branch_ == old_b) {
        branch_ = new_b;
    }
    RefreshAsync();
    return true;
}

bool GitManager::MergeBranch(const std::string& branch_name, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string b = Trim(branch_name);
    if (b.empty()) {
        out_error = "Branch name cannot be empty.";
        return false;
    }

    WaitForPendingTasks();
    ApplyPendingUpdates();

    auto res = RunGit("merge \"" + b + "\"");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to merge branch: " + b;
        return false;
    }
    return true;
}

std::vector<std::string> GitManager::GetRemoteList() const {
    std::vector<std::string> remotes;
    if (!has_repo_) return remotes;

    auto res = RunGit("remote");
    if (res.exit_code != 0) return remotes;

    std::istringstream stream(res.output);
    std::string line;
    while (std::getline(stream, line)) {
        std::string r = Trim(line);
        if (!r.empty()) remotes.push_back(r);
    }
    return remotes;
}

bool GitManager::RemoveRemote(const std::string& remote_name, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string r = Trim(remote_name);
    if (r.empty()) {
        out_error = "Remote name cannot be empty.";
        return false;
    }

    auto res = RunGit("remote remove \"" + r + "\"");
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        return false;
    }
    RefreshAsync();
    return true;
}

bool GitManager::PushForce(std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    if (branch_.empty() || branch_.starts_with("HEAD")) {
        out_error = "Cannot push in detached HEAD state.";
        return false;
    }

    auto res = RunGit("push --force-with-lease origin \"" + branch_ + "\"");
    if (res.exit_code != 0) {
        res = RunGit("push --force origin \"" + branch_ + "\"");
    }

    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git force push failed.";
        return false;
    }
    return true;
}

std::vector<GitStashEntry> GitManager::GetStashList() const {
    std::vector<GitStashEntry> stashes;
    if (!has_repo_) return stashes;

    auto res = RunGit("stash list");
    if (res.exit_code != 0) return stashes;

    std::istringstream stream(res.output);
    std::string line;
    while (std::getline(stream, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty()) continue;

        GitStashEntry entry;
        auto at_pos = trimmed.find("stash@{");
        if (at_pos != std::string::npos) {
            auto end_pos = trimmed.find('}', at_pos);
            if (end_pos != std::string::npos) {
                std::string idx_str = trimmed.substr(at_pos + 7, end_pos - at_pos - 7);
                try {
                    entry.index = std::stoi(idx_str);
                } catch (...) {
                    entry.index = 0;
                }
                entry.name = trimmed.substr(at_pos, end_pos - at_pos + 1);
                size_t colon_pos = trimmed.find(':', end_pos);
                if (colon_pos != std::string::npos) {
                    entry.message = Trim(trimmed.substr(colon_pos + 1));
                } else {
                    entry.message = trimmed.substr(end_pos + 1);
                }
                stashes.push_back(std::move(entry));
            }
        }
    }
    return stashes;
}

bool GitManager::StashSave(const std::string& message, bool include_untracked, bool keep_index, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    std::string cmd = "stash push";
    if (include_untracked) cmd += " -u";
    if (keep_index) cmd += " --keep-index";
    std::string msg = Trim(message);
    if (!msg.empty()) {
        std::string escaped;
        for (char c : msg) {
            if (c == '"') escaped += "\\\"";
            else escaped += c;
        }
        cmd += " -m \"" + escaped + "\"";
    }

    auto res = RunGit(cmd);
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git stash failed.";
        return false;
    }
    return true;
}

bool GitManager::StashPop(int index, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    auto res = RunGit("stash pop \"stash@{" + std::to_string(index) + "}\"");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git stash pop failed.";
        return false;
    }
    return true;
}

bool GitManager::StashApply(int index, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    auto res = RunGit("stash apply \"stash@{" + std::to_string(index) + "}\"");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git stash apply failed.";
        return false;
    }
    return true;
}

bool GitManager::StashDrop(int index, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    WaitForPendingTasks();
    ApplyPendingUpdates();

    auto res = RunGit("stash drop \"stash@{" + std::to_string(index) + "}\"");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git stash drop failed.";
        return false;
    }
    return true;
}

std::vector<std::string> GitManager::GetTagList() const {
    std::vector<std::string> tags;
    if (!has_repo_) return tags;

    auto res = RunGit("tag -l");
    if (res.exit_code != 0) return tags;

    std::istringstream stream(res.output);
    std::string line;
    while (std::getline(stream, line)) {
        std::string t = Trim(line);
        if (!t.empty()) tags.push_back(t);
    }
    return tags;
}

bool GitManager::CreateTag(const std::string& tag_name, const std::string& message, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string t = Trim(tag_name);
    if (t.empty()) {
        out_error = "Tag name cannot be empty.";
        return false;
    }

    std::string cmd;
    std::string msg = Trim(message);
    if (msg.empty()) {
        cmd = "tag \"" + t + "\"";
    } else {
        std::string escaped;
        for (char c : msg) {
            if (c == '"') escaped += "\\\"";
            else escaped += c;
        }
        cmd = "tag -a \"" + t + "\" -m \"" + escaped + "\"";
    }

    auto res = RunGit(cmd);
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to create tag: " + t;
        return false;
    }
    return true;
}

bool GitManager::DeleteTag(const std::string& tag_name, std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    std::string t = Trim(tag_name);
    if (t.empty()) {
        out_error = "Tag name cannot be empty.";
        return false;
    }

    auto res = RunGit("tag -d \"" + t + "\"");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to delete tag: " + t;
        return false;
    }
    return true;
}

bool GitManager::PushTags(std::string& out_error) {
    if (!has_repo_) {
        out_error = "No Git repository found.";
        return false;
    }
    auto res = RunGit("push --tags");
    RefreshAsync();
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Failed to push tags.";
        return false;
    }
    return true;
}

bool GitManager::CloneRepo(const std::string& url, const std::string& target_path, std::string& out_error) {
    std::string u = Trim(url);
    std::string tp = Trim(target_path);
    if (u.empty() || tp.empty()) {
        out_error = "URL and target path cannot be empty.";
        return false;
    }

    std::string win_target = tp;
    std::ranges::replace(win_target, '/', '\\');

    auto res = platform::RunCommand("git clone \"" + u + "\" \"" + win_target + "\"");
    if (res.exit_code != 0) {
        out_error = Trim(res.output);
        if (out_error.empty()) out_error = "Git clone failed.";
        return false;
    }
    return true;
}

}  // namespace luce
