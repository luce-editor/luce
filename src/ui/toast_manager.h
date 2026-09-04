#pragma once
// ============================================================================
// ToastManager — VS Code-style floating popup notifications.
//
// Displays dismissable notification cards in the lower corner of the editor
// (above the status bar) with smooth fade-in/fade-out, custom icons, and
// auto-expiration.
// ============================================================================

#include "theme.h"
#include <string>
#include <vector>
#include <chrono>

namespace luce {

enum class ToastType {
    Info,
    Warning,
    Error,
    Success
};

struct Toast {
    uint64_t id = 0;
    std::string message;
    ToastType type = ToastType::Info;
    float time_remaining = 5.0f;
    float total_duration = 5.0f;
    bool dismissed = false;
};

class ToastManager {
public:
    ToastManager() = default;

    /// Push a new toast notification.
    void Show(std::string message, ToastType type = ToastType::Info, float duration_seconds = 5.0f);
    void ShowInfo(std::string message, float duration = 5.0f) { Show(std::move(message), ToastType::Info, duration); }
    void ShowWarning(std::string message, float duration = 6.0f) { Show(std::move(message), ToastType::Warning, duration); }
    void ShowError(std::string message, float duration = 8.0f) { Show(std::move(message), ToastType::Error, duration); }
    void ShowSuccess(std::string message, float duration = 5.0f) { Show(std::move(message), ToastType::Success, duration); }

    /// Render any active toast notifications. Called once per frame in App::Render().
    void Render(const Theme& theme);

    /// Clear all active toasts immediately.
    void Clear();

private:
    std::vector<Toast> toasts_;
    uint64_t next_id_ = 1;
    std::chrono::steady_clock::time_point last_frame_time_ = std::chrono::steady_clock::now();
};

}  // namespace luce
