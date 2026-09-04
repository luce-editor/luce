// ============================================================================
// Platform — Windows implementation of OS abstraction layer.
// ============================================================================

#include "platform.h"

#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#   include <commdlg.h>
#   include <shlobj.h>
#   include <shobjidl.h>
#   include <shellapi.h>
#else
#   include <dlfcn.h>
#   include <signal.h>
#   include <sys/wait.h>
#   include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace luce::platform {

// ── File dialogs ──────────────────────────────────────────────────────────

#ifdef _WIN32

std::string OpenFileDialog(const char* filter) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn       = {};
    ofn.lStructSize         = sizeof(ofn);
    ofn.lpstrFilter         = filter;
    ofn.lpstrFile           = filename;
    ofn.nMaxFile            = MAX_PATH;
    ofn.Flags               = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        std::string result(filename);
        std::ranges::replace(result, '\\', '/');
        return result;
    }
    return {};
}

std::string SaveFileDialog(const char* filter) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn       = {};
    ofn.lStructSize         = sizeof(ofn);
    ofn.lpstrFilter         = filter;
    ofn.lpstrFile           = filename;
    ofn.nMaxFile            = MAX_PATH;
    ofn.Flags               = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        std::string result(filename);
        std::ranges::replace(result, '\\', '/');
        return result;
    }
    return {};
}

std::string OpenFolderDialog() {
    std::string result;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool need_uninit = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog,
                            reinterpret_cast<void**>(&dialog));
    if (SUCCEEDED(hr)) {
        DWORD options;
        if (SUCCEEDED(dialog->GetOptions(&options))) {
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        dialog->SetTitle(L"Select Folder");

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path_w = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path_w))) {
                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, path_w, -1, nullptr, 0, nullptr, nullptr);
                    if (size_needed > 0) {
                        result.resize(size_needed - 1);
                        WideCharToMultiByte(CP_UTF8, 0, path_w, -1, result.data(), size_needed, nullptr, nullptr);
                        std::ranges::replace(result, '\\', '/');
                    }
                    CoTaskMemFree(path_w);
                }
                item->Release();
            }
        }
        dialog->Release();
    } else {
        // Fallback for older systems without IFileOpenDialog
        char path[MAX_PATH] = {};
        BROWSEINFOA bi      = {};
        bi.lpszTitle        = "Select Folder";
        bi.ulFlags          = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl && SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            result = path;
            std::ranges::replace(result, '\\', '/');
        } else if (pidl) {
            CoTaskMemFree(pidl);
        }
    }

    if (need_uninit) {
        CoUninitialize();
    }

    return result;
}

#else  // POSIX stub — simple fallback using zenity if available

std::string OpenFileDialog(const char*) {
    // TODO: implement via zenity / kdialog / native macOS API
    return {};
}

std::string SaveFileDialog(const char*) { return {}; }
std::string OpenFolderDialog() { return {}; }

#endif

// ── Filesystem helpers ────────────────────────────────────────────────────

std::string GetDirectory(const std::string& path) {
    auto p = fs::path(path).parent_path().string();
    std::ranges::replace(p, '\\', '/');
    return p;
}

std::string GetFilename(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string GetExtension(const std::string& path) {
    return fs::path(path).extension().string();
}

std::string GetExecutableDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string result = fs::path(buf).parent_path().string();
    std::ranges::replace(result, '\\', '/');
    return result;
#else
    return fs::read_symlink("/proc/self/exe").parent_path().string();
#endif
}

void OpenInFileExplorer(const std::string& path) {
#ifdef _WIN32
    std::string win_path = path;
    std::ranges::replace(win_path, '/', '\\');

    std::error_code ec;
    if (fs::is_regular_file(win_path, ec)) {
        std::string param = "/select,\"" + win_path + "\"";
        ShellExecuteA(nullptr, "open", "explorer.exe", param.c_str(), nullptr, SW_SHOW);
    } else {
        ShellExecuteA(nullptr, "open", win_path.c_str(), nullptr, nullptr, SW_SHOW);
    }
#elif __APPLE__
    std::string cmd = "open -R \"" + path + "\"";
    system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + path + "\"";
    system(cmd.c_str());
#endif
}

// ── Dynamic library loading ───────────────────────────────────────────────

#ifdef _WIN32

LibraryHandle LoadDynamicLibrary(const char* path) {
    return static_cast<LibraryHandle>(LoadLibraryA(path));
}

void FreeDynamicLibrary(LibraryHandle handle) {
    if (handle) FreeLibrary(static_cast<HMODULE>(handle));
}

void* GetSymbol(LibraryHandle handle, const char* name) {
    if (!handle) return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

#else

LibraryHandle LoadDynamicLibrary(const char* path) {
    return dlopen(path, RTLD_NOW);
}

void FreeDynamicLibrary(LibraryHandle handle) {
    if (handle) dlclose(handle);
}

void* GetSymbol(LibraryHandle handle, const char* name) {
    if (!handle) return nullptr;
    return dlsym(handle, name);
}

#endif

CommandResult RunCommand(const std::string& command, const std::string& cwd) {
    CommandResult res;
#ifdef _WIN32
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return res;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi = {};
    std::string cmd = "cmd.exe /c " + command;
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    const char* working_dir = cwd.empty() ? nullptr : cwd.c_str();
    if (CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, working_dir, &si, &pi)) {
        CloseHandle(hWritePipe);
        hWritePipe = NULL;

        char buffer[1024];
        DWORD bytes_read = 0;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            res.output.append(buffer, bytes_read);
        }

        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        res.exit_code = static_cast<int>(exit_code);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    if (hWritePipe) CloseHandle(hWritePipe);
    if (hReadPipe) CloseHandle(hReadPipe);
#else
    std::string full_cmd = cwd.empty() ? command : "cd \"" + cwd + "\" && " + command + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) {
            res.output += buf;
        }
        res.exit_code = pclose(pipe);
    }
#endif
    return res;
}

// ── Process (interactive subprocess with piped I/O) ───────────────────────

#ifdef _WIN32
#include <consoleapi.h>

/// Windows implementation using CreatePseudoConsole (ConPTY).
struct Process::Impl {
    HANDLE h_process       = INVALID_HANDLE_VALUE;
    HANDLE stdin_write     = INVALID_HANDLE_VALUE;
    HANDLE stdout_read     = INVALID_HANDLE_VALUE;
    HPCON  h_pty           = nullptr;

    ~Impl() { Cleanup(); }

    void Cleanup() {
        auto safe_close = [](HANDLE& h) {
            if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; }
        };
        safe_close(stdin_write);
        safe_close(stdout_read);
        safe_close(h_process);
        if (h_pty) {
            ClosePseudoConsole(h_pty);
            h_pty = nullptr;
        }
    }
};

Process::Process()  = default;
Process::~Process() { Kill(); delete impl_; }

bool Process::Start(const std::string& command, int cols, int rows, const std::string& cwd) {
    if (impl_) { Kill(); delete impl_; }
    impl_ = new Impl();

    HANDLE hPipeInRead = INVALID_HANDLE_VALUE;
    HANDLE hPipeInWrite = INVALID_HANDLE_VALUE;
    HANDLE hPipeOutRead = INVALID_HANDLE_VALUE;
    HANDLE hPipeOutWrite = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&hPipeInRead, &hPipeInWrite, nullptr, 0)) return false;
    if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, nullptr, 0)) {
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        return false;
    }

    COORD size = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    HRESULT hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &impl_->h_pty);
    if (FAILED(hr)) {
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        CloseHandle(hPipeOutWrite);
        return false;
    }

    impl_->stdin_write = hPipeInWrite;
    impl_->stdout_read = hPipeOutRead;

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    LPPROC_THREAD_ATTRIBUTE_LIST attrList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrListSize);
    InitializeProcThreadAttributeList(attrList, 1, 0, &attrListSize);
    UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, impl_->h_pty, sizeof(HPCON), nullptr, nullptr);

    STARTUPINFOEXA siex = {};
    siex.StartupInfo.cb = sizeof(STARTUPINFOEXA);
    siex.lpAttributeList = attrList;

    PROCESS_INFORMATION pi = {};
    std::string cmd_copy = command;
    const char* working_dir = cwd.empty() ? nullptr : cwd.c_str();

    BOOL ok = CreateProcessA(
        nullptr, cmd_copy.data(), nullptr, nullptr,
        FALSE, EXTENDED_STARTUPINFO_PRESENT, nullptr, working_dir,
        &siex.StartupInfo, &pi);

    free(attrList);
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeOutWrite);

    if (!ok) {
        return false;
    }

    impl_->h_process = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

void Process::Resize(int cols, int rows) {
    if (impl_ && impl_->h_pty) {
        COORD size = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
        ResizePseudoConsole(impl_->h_pty, size);
    }
}

bool Process::Write(const std::string& data) {
    if (!impl_ || impl_->stdin_write == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(impl_->stdin_write, data.c_str(),
                        static_cast<DWORD>(data.size()), &written, nullptr);
    FlushFileBuffers(impl_->stdin_write);
    return ok != 0;
}

/// Non-blocking read from the child's stdout.  Uses PeekNamedPipe to avoid
/// blocking the main thread when no data is available.
std::string Process::Read() {
    if (!impl_ || impl_->stdout_read == INVALID_HANDLE_VALUE) return {};

    std::string result;
    char chunk[4096];
    while (true) {
        DWORD available = 0;
        if (!PeekNamedPipe(impl_->stdout_read, nullptr, 0, nullptr, &available, nullptr) || available == 0) {
            break;
        }

        DWORD to_read = std::min<DWORD>(available, sizeof(chunk));
        DWORD bytes_read = 0;
        if (ReadFile(impl_->stdout_read, chunk, to_read, &bytes_read, nullptr) && bytes_read > 0) {
            result.append(chunk, bytes_read);
        } else {
            break;
        }
    }
    return result;
}

bool Process::IsRunning() const {
    if (!impl_ || impl_->h_process == INVALID_HANDLE_VALUE) return false;
    DWORD exit_code = 0;
    GetExitCodeProcess(impl_->h_process, &exit_code);
    return exit_code == STILL_ACTIVE;
}

void Process::Kill() {
    if (impl_ && impl_->h_process != INVALID_HANDLE_VALUE) {
        TerminateProcess(impl_->h_process, 1);
        impl_->Cleanup();
    }
}

#else  // POSIX — fork + exec with pipes

struct Process::Impl {
    pid_t pid       = -1;
    int   stdin_fd  = -1;
    int   stdout_fd = -1;
    ~Impl() { Cleanup(); }
    void Cleanup() {
        if (stdin_fd  >= 0) { close(stdin_fd);  stdin_fd  = -1; }
        if (stdout_fd >= 0) { close(stdout_fd); stdout_fd = -1; }
    }
};

Process::Process()  = default;
Process::~Process() { Kill(); delete impl_; }

bool Process::Start(const std::string& command) {
    if (impl_) { Kill(); delete impl_; }
    impl_ = new Impl();

    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) return false;

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        // Child
        dup2(in_pipe[0],  STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[1]); close(out_pipe[0]);
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent
    close(in_pipe[0]);
    close(out_pipe[1]);
    impl_->pid       = pid;
    impl_->stdin_fd  = in_pipe[1];
    impl_->stdout_fd = out_pipe[0];

    // Make stdout non-blocking
    int flags = fcntl(impl_->stdout_fd, F_GETFL, 0);
    fcntl(impl_->stdout_fd, F_SETFL, flags | O_NONBLOCK);
    return true;
}

bool Process::Write(const std::string& data) {
    if (!impl_ || impl_->stdin_fd < 0) return false;
    return write(impl_->stdin_fd, data.c_str(), data.size()) >= 0;
}

std::string Process::Read() {
    if (!impl_ || impl_->stdout_fd < 0) return {};
    char buf[4096];
    ssize_t n = read(impl_->stdout_fd, buf, sizeof(buf));
    if (n > 0) return std::string(buf, n);
    return {};
}

bool Process::IsRunning() const {
    if (!impl_ || impl_->pid <= 0) return false;
    return waitpid(impl_->pid, nullptr, WNOHANG) == 0;
}

void Process::Kill() {
    if (impl_ && impl_->pid > 0) {
        kill(impl_->pid, SIGTERM);
        waitpid(impl_->pid, nullptr, 0);
        impl_->pid = -1;
        impl_->Cleanup();
    }
}

#endif

}  // namespace luce::platform
