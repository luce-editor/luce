#include "toast_manager.h"
#include <imgui.h>
#include <algorithm>

namespace luce {

void ToastManager::Show(std::string message, ToastType type, float duration_seconds) {
    // Limit queue to at most 4 visible toasts
    if (toasts_.size() >= 4) {
        toasts_.erase(toasts_.begin());
    }

    Toast toast;
    toast.id = next_id_++;
    toast.message = std::move(message);
    toast.type = type;
    toast.time_remaining = duration_seconds;
    toast.total_duration = duration_seconds;
    toast.dismissed = false;
    toasts_.push_back(std::move(toast));
}

void ToastManager::Clear() {
    toasts_.clear();
}

void ToastManager::Render(const Theme& theme) {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;

    // Clamp dt in case of stalls
    dt = std::clamp(dt, 0.0f, 0.1f);

    // Update lifetimes and remove expired
    for (auto& toast : toasts_) {
        toast.time_remaining -= dt;
        if (toast.time_remaining <= 0.0f) {
            toast.dismissed = true;
        }
    }

    std::erase_if(toasts_, [](const Toast& t) { return t.dismissed; });

    if (toasts_.empty()) {
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float statusbar_height = ImGui::GetFrameHeight() + 4.0f;
    float start_x = vp->WorkPos.x + 16.0f;
    float current_y = vp->WorkPos.y + vp->WorkSize.y - statusbar_height - 12.0f;
    float max_toast_width = std::min(450.0f, vp->WorkSize.x - 32.0f);

    // Render each toast stacking upwards from bottom-left
    for (int i = static_cast<int>(toasts_.size()) - 1; i >= 0; --i) {
        auto& toast = toasts_[i];

        // Fade calculation
        float fade_in = std::clamp((toast.total_duration - toast.time_remaining) / 0.25f, 0.0f, 1.0f);
        float fade_out = std::clamp(toast.time_remaining / 0.4f, 0.0f, 1.0f);
        float alpha = std::min(fade_in, fade_out);

        ImVec4 icon_color;
        const char* icon_str = "(i)";
        ImVec4 border_color = ImVec4(0.25f, 0.25f, 0.25f, alpha);

        switch (toast.type) {
            case ToastType::Error:
                icon_color = ImVec4(0.95f, 0.32f, 0.32f, alpha);
                icon_str = "\xEF\x9C\x95"; // Cross icon or fallback
                icon_str = "(x)";
                border_color = ImVec4(0.85f, 0.25f, 0.25f, alpha * 0.7f);
                break;
            case ToastType::Warning:
                icon_color = ImVec4(0.95f, 0.72f, 0.20f, alpha);
                icon_str = "(!)";
                border_color = ImVec4(0.85f, 0.65f, 0.15f, alpha * 0.7f);
                break;
            case ToastType::Success:
                icon_color = ImVec4(0.28f, 0.78f, 0.45f, alpha);
                icon_str = "(v)";
                border_color = ImVec4(0.25f, 0.70f, 0.40f, alpha * 0.7f);
                break;
            case ToastType::Info:
            default:
                icon_color = ImVec4(0.22f, 0.65f, 0.95f, alpha);
                icon_str = "(i)";
                border_color = ImVec4(0.20f, 0.55f, 0.85f, alpha * 0.7f);
                break;
        }

        ImVec4 bg_color = ImVec4(theme.sidebar_bg.x * 0.85f, theme.sidebar_bg.y * 0.85f, theme.sidebar_bg.z * 0.85f, alpha * 0.96f);
        ImVec4 text_color = ImVec4(theme.foreground.x, theme.foreground.y, theme.foreground.z, alpha);

        std::string window_name = "##toast_" + std::to_string(toast.id);

        // Estimate toast height and position above current_y
        ImGui::SetNextWindowPos(ImVec2(start_x, current_y), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(240.0f, 0.0f), ImVec2(max_toast_width, 200.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color);
        ImGui::PushStyleColor(ImGuiCol_Border, border_color);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin(window_name.c_str(), nullptr, flags)) {
            // Pause timer if hovered so user can comfortably read long messages
            if (ImGui::IsWindowHovered()) {
                toast.time_remaining = std::max(toast.time_remaining, 2.0f);
            }

            // Status Icon
            ImGui::TextColored(icon_color, "%s", icon_str);
            ImGui::SameLine(0, 8.0f);

            // Message text with wrapping
            float content_width = ImGui::GetWindowWidth() - 52.0f;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + content_width);
            ImGui::TextUnformatted(toast.message.c_str());
            ImGui::PopTextWrapPos();

            // Dismiss 'x' button
            ImGui::SameLine(ImGui::GetWindowWidth() - 24.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(text_color.x, text_color.y, text_color.z, alpha * 0.7f));
            std::string btn_id = "x##toast_close_" + std::to_string(toast.id);
            if (ImGui::SmallButton(btn_id.c_str())) {
                toast.dismissed = true;
            }
            ImGui::PopStyleColor(4);

            // Update current_y for the next toast in the stack
            float this_height = ImGui::GetWindowHeight();
            current_y -= (this_height + 6.0f);
        }
        ImGui::End();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }
}

}  // namespace luce
