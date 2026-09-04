#pragma once
// ============================================================================
// FileExplorer — Tree-view sidebar for browsing the project directory.
// ============================================================================

#include <functional>
#include <string>
#include <vector>

namespace luce {

class FileExplorer {
public:
    using OpenFileCallback = std::function<void(const std::string&)>;
    using ActionCallback   = std::function<void()>;

    FileExplorer();

    /// Set the root directory to display.
    void SetRoot(const std::string& path);

    /// Register callbacks.
    void SetOnOpenFile(OpenFileCallback cb)       { on_open_file_ = std::move(cb); }
    void SetOnOpenFolder(ActionCallback cb)       { on_open_folder_ = std::move(cb); }
    void SetOnRemoveFolder(ActionCallback cb)     { on_remove_folder_ = std::move(cb); }

    /// Render the file tree panel.
    void Render();

    const std::string& Root() const { return root_; }

private:
    /// Recursive helper that draws one level of the tree.
    void RenderDirectory(const std::string& path);
    void RenderModals();

    std::string      root_;
    OpenFileCallback on_open_file_;
    ActionCallback   on_open_folder_;
    ActionCallback   on_remove_folder_;
    char             filter_buf_[128] = {};

    // Context action state
    std::string      target_path_;
    std::string      target_dir_;
    char             action_buf_[256] = {};
    bool             show_new_file_modal_ = false;
    bool             show_new_folder_modal_ = false;
    bool             show_rename_modal_ = false;
    bool             show_delete_modal_ = false;
};

}  // namespace luce
