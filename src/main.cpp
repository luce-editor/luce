// ============================================================================
// main.cpp — Entry point for the Luce code editor.
//
// Initialises SDL2, creates an OpenGL 3.3 context, sets up Dear ImGui
// (with docking enabled), loads fonts, and runs the main render loop.
// ============================================================================

#include "ui/app.h"
#include "ui/icon_manager.h"
#include "platform.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "misc/freetype/imgui_freetype.h"

#include <SDL.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#   include <SDL_opengl.h>
#   include <SDL_syswm.h>
#   include <dwmapi.h>
#   ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#       define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#   endif
#else
#   include <SDL_opengl.h>
#endif

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    // ── Initialise SDL2 ───────────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // Request OpenGL 3.3 Core Profile.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow(
        "Luce — Code Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1440, 900,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIDDEN);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Use the app's native Windows icon resource instead of overriding it with
    // a PNG surface, which can render incorrectly in the taskbar on some setups.
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);  // VSync on.

    // Pre-clear backbuffer with default dark background
    glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // ── Dark title bar (Windows 10 1809+) ─────────────────────────────────
#if defined(_WIN32)
    {
        SDL_SysWMinfo wm_info;
        SDL_VERSION(&wm_info.version);
        if (SDL_GetWindowWMInfo(window, &wm_info)) {
            HWND hwnd = wm_info.info.win.window;
            BOOL use_dark = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &use_dark, sizeof(use_dark));
        }
    }
#endif

    // Enable drag-and-drop.
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    // ── Initialise Dear ImGui ─────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable docking.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;                 // Prevent internal panels from spawning OS windows
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    const char* glsl_version = "#version 330 core";
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ── Load fonts ────────────────────────────────────────────────────────
    //
    // 1. IBM Plex Sans: UI, Menus, Dialogs, Markdown Preview
    // 2. Lilex: Monospace Editor Code & Terminal
    
    std::string exe_dir = luce::platform::GetExecutableDir();
    
    std::vector<std::string> ibm_search_dirs = {
        exe_dir + "/assets/fonts/IBM_Plex_Sans/static",
        exe_dir + "/../assets/fonts/IBM_Plex_Sans/static",
        "assets/fonts/IBM_Plex_Sans/static"
    };

    std::vector<std::string> lilex_search_dirs = {
        exe_dir + "/assets/fonts/Lilex/ttf",
        exe_dir + "/../assets/fonts/Lilex/ttf",
        "assets/fonts/Lilex/ttf"
    };

    std::string ibm_dir;
    for (const auto& d : ibm_search_dirs) {
        if (fs::exists(d + "/IBMPlexSans-Regular.ttf")) {
            ibm_dir = d;
            break;
        }
    }

    std::string lilex_dir;
    for (const auto& d : lilex_search_dirs) {
        if (fs::exists(d + "/Lilex-Regular.ttf")) {
            lilex_dir = d;
            break;
        }
    }

    float base_font_size = 15.0f;

    // High-DPI: scale the font size.
    float dpi_scale = 1.0f;
    {
        int display_index = SDL_GetWindowDisplayIndex(window);
        float ddpi = 0, hdpi = 0, vdpi = 0;
        if (SDL_GetDisplayDPI(display_index, &ddpi, &hdpi, &vdpi) == 0 && ddpi > 0) {
            dpi_scale = ddpi / 96.0f;
            if (dpi_scale > 1.0f) base_font_size *= dpi_scale;
        }
    }

    ImFont* font_regular     = nullptr; // IBM Plex Sans UI Default
    ImFont* font_editor_mono = nullptr; // Lilex Code Monospace
    ImFont* font_bold        = nullptr; // IBM Plex Sans Bold
    ImFont* font_italic      = nullptr; // IBM Plex Sans Italic
    ImFont* font_h1          = nullptr; // IBM Plex Sans H1
    ImFont* font_h2          = nullptr; // IBM Plex Sans H2

    static const ImWchar glyph_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0100, 0x024F, // Latin Extended-A + Latin Extended-B (e.g. Polish characters)
        0x2000, 0x206F, // General Punctuation (including • U+2022 bullet)
        0x2190, 0x21FF, // Arrows (including ↺, ↻, etc.)
        0x25A0, 0x25FF, // Geometric Shapes (including ● U+25CF circle)
        0
    };

    // Locate system or bundled emoji font
    std::string emoji_font_path;
#ifdef _WIN32
    std::string win_dir = getenv("WINDIR") ? getenv("WINDIR") : (getenv("SystemRoot") ? getenv("SystemRoot") : "C:/Windows");
    std::vector<std::string> emoji_candidates = {
        win_dir + "/Fonts/seguiemj.ttf", // Segoe UI Emoji (full color)
        win_dir + "/Fonts/seguisym.ttf", // Segoe UI Symbol
        exe_dir + "/assets/fonts/seguiemj.ttf",
        "assets/fonts/seguiemj.ttf"
    };
#else
    std::vector<std::string> emoji_candidates = {
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/google-noto-color-emoji/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/ancient-scripts/Symbola.ttf",
        exe_dir + "/assets/fonts/NotoColorEmoji.ttf",
        "assets/fonts/NotoColorEmoji.ttf"
    };
#endif
    for (const auto& candidate : emoji_candidates) {
        if (fs::exists(candidate)) {
            emoji_font_path = candidate;
            printf("Found Emoji Font: %s\n", candidate.c_str());
            break;
        }
    }

    // Emoji glyph ranges covering Dingbats, Miscellaneous Symbols, and SMP Emoji planes
    static const ImWchar emoji_glyph_ranges[] = {
        0x2000, 0x27BF,   // Punctuation, Currency, Arrows, Tech (⌨️), Shapes, Symbols (⚡), Dingbats (✨)
        0x2B00, 0x2BFF,   // Misc Symbols and Arrows
        0x2900, 0x297F,   // Supplemental Arrows-B
        0x1F300, 0x1F64F, // Miscellaneous Symbols and Pictographs, Emoticons (😀 - 😶)
        0x1F680, 0x1F6FF, // Transport and Map Symbols (🚀, 🛸, etc.)
        0x1F900, 0x1F9FF, // Supplemental Symbols and Pictographs (🧩, 🧱, 🧭, 🧪, etc.)
        0
    };

    std::vector<uint8_t> emoji_file_bytes;
    if (!emoji_font_path.empty()) {
        std::ifstream f(emoji_font_path, std::ios::binary | std::ios::ate);
        if (f.is_open()) {
            std::streamsize size = f.tellg();
            f.seekg(0, std::ios::beg);
            emoji_file_bytes.resize(size);
            f.read(reinterpret_cast<char*>(emoji_file_bytes.data()), size);
        }
    }

    ImFontConfig emoji_cfg;
    emoji_cfg.MergeMode = true;
    emoji_cfg.OversampleH = 1;
    emoji_cfg.OversampleV = 1;
    emoji_cfg.FontDataOwnedByAtlas = false;
    emoji_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;

    // Load UI font first so it becomes ImGui's default font
    if (!ibm_dir.empty()) {
        std::string regular_path  = ibm_dir + "/IBMPlexSans-Regular.ttf";
        std::string bold_path     = ibm_dir + "/IBMPlexSans-Bold.ttf";
        std::string italic_path   = ibm_dir + "/IBMPlexSans-Italic.ttf";
        std::string semibold_path = ibm_dir + "/IBMPlexSans-SemiBold.ttf";

        font_regular = io.Fonts->AddFontFromFileTTF(regular_path.c_str(), base_font_size, nullptr, glyph_ranges);
        if (!emoji_file_bytes.empty()) {
            io.Fonts->AddFontFromMemoryTTF(emoji_file_bytes.data(), static_cast<int>(emoji_file_bytes.size()), base_font_size, &emoji_cfg, emoji_glyph_ranges);
        }

        font_bold    = io.Fonts->AddFontFromFileTTF(bold_path.c_str(), base_font_size, nullptr, glyph_ranges);
        font_italic  = io.Fonts->AddFontFromFileTTF(italic_path.c_str(), base_font_size, nullptr, glyph_ranges);
        font_h1      = io.Fonts->AddFontFromFileTTF(bold_path.c_str(), base_font_size * 1.6f, nullptr, glyph_ranges);
        font_h2      = io.Fonts->AddFontFromFileTTF(semibold_path.c_str(), base_font_size * 1.3f, nullptr, glyph_ranges);

        printf("Loaded UI Font: IBM Plex Sans with Emoji support\n");
    }

    // Load Lilex for editor and terminal
    if (!lilex_dir.empty()) {
        std::string lilex_path = lilex_dir + "/Lilex-Regular.ttf";
        font_editor_mono = io.Fonts->AddFontFromFileTTF(lilex_path.c_str(), base_font_size, nullptr, glyph_ranges);
        if (!emoji_file_bytes.empty()) {
            io.Fonts->AddFontFromMemoryTTF(emoji_file_bytes.data(), static_cast<int>(emoji_file_bytes.size()), base_font_size, &emoji_cfg, emoji_glyph_ranges);
        }
        printf("Loaded Editor Font: Lilex Monospace (%s) with Emoji support\n", lilex_path.c_str());
    } else if (font_regular) {
        font_editor_mono = font_regular;
    }

    // Initialise IconManager with the assets/icons/ directory
    std::vector<std::string> icons_search_dirs = {
        exe_dir + "/assets/icons",
        exe_dir + "/../assets/icons",
        exe_dir + "/../../assets/icons",
        "assets/icons"
    };
    std::string custom_icons_dir;
    std::vector<std::string> custom_icons_candidates = {
        exe_dir + "/icons",
        exe_dir + "/../icons",
        exe_dir + "/../../icons",
        "icons"
    };
    for (const auto& c : custom_icons_candidates) {
        if (fs::exists(c)) {
            custom_icons_dir = c;
            break;
        }
    }

    for (const auto& d : icons_search_dirs) {
        if (fs::exists(d + "/file_type_cpp.svg")) {
            luce::IconManager::Instance().Init(d, custom_icons_dir);
            printf("Loaded Icons from: %s (custom: %s)\n", d.c_str(), custom_icons_dir.c_str());
            break;
        }
    }

    // Optimize font atlas packing to minimize RAM usage
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;

    // ── Create the application ────────────────────────────────────────────
    luce::App app;
    app.SetFonts(font_regular, font_editor_mono, font_bold, font_italic, font_h1, font_h2);

    // If a file/folder was passed as a CLI argument, open it.
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        std::ranges::replace(arg, '\\', '/');
        app.OnFileDrop(arg);
    }

    // ── Main loop (Optimized for 0.0% idle CPU & low latency) ─────────────
    bool running = true;
    bool window_focused = true;
    int active_frames_remaining = 30; // Initialize UI smoothly

    while (running) {
        // When completely idle, sleep in kernel wait instead of spinning CPU
        if (active_frames_remaining <= 0) {
            int wait_timeout_ms = window_focused ? 50 : 200;
            SDL_Event wait_event;
            if (SDL_WaitEventTimeout(&wait_event, wait_timeout_ms)) {
                // Event woke us up immediately with 0ms latency!
                ImGui_ImplSDL2_ProcessEvent(&wait_event);
                if (wait_event.type == SDL_QUIT) running = false;
                if (wait_event.type == SDL_WINDOWEVENT) {
                    if (wait_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                        window_focused = true;
                        app.OnFocusGained();
                    } else if (wait_event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                        if (SDL_GetKeyboardFocus() == nullptr) {
                            window_focused = false;
#if defined(_WIN32)
                            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
#endif
                        }
                    } else if (wait_event.window.event == SDL_WINDOWEVENT_CLOSE && wait_event.window.windowID == SDL_GetWindowID(window)) {
                        running = false;
                    }
                } else if (wait_event.type == SDL_DROPFILE) {
                    std::string path(wait_event.drop.file);
                    std::ranges::replace(path, '\\', '/');
                    app.OnFileDrop(path);
                    SDL_free(wait_event.drop.file);
                }
                active_frames_remaining = 20; // 20 frames of smooth rendering after event
            }
        }

        // Process all queued events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            active_frames_remaining = 20;

            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                        window_focused = true;
                        app.OnFocusGained();
                    } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                        if (SDL_GetKeyboardFocus() == nullptr) {
                            window_focused = false;
#if defined(_WIN32)
                            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
#endif
                        }
                    } else if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
                               event.window.windowID == SDL_GetWindowID(window)) {
                        running = false;
                    }
                    break;

                case SDL_DROPFILE: {
                    std::string path(event.drop.file);
                    std::ranges::replace(path, '\\', '/');
                    app.OnFileDrop(path);
                    SDL_free(event.drop.file);
                    break;
                }
            }
        }

        if (active_frames_remaining > 0) {
            --active_frames_remaining;
        }

        if (app.WantsQuit()) running = false;

        // ── New frame ─────────────────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ── Application rendering ─────────────────────────────────────────
        app.Render();

        // ── OpenGL rendering ──────────────────────────────────────────────
        ImGui::Render();

        static bool trimmed_working_set = false;
        if (!trimmed_working_set && active_frames_remaining == 0) {
            trimmed_working_set = true;
#if defined(_WIN32)
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
#endif
        }

        int display_w, display_h;
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        
        ImVec4 bg = app.GetBackgroundColor();
        glClearColor(bg.x, bg.y, bg.z, bg.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);

#if defined(_WIN32)
            for (int i = 1; i < ImGui::GetPlatformIO().Viewports.Size; i++) {
                ImGuiViewport* vp = ImGui::GetPlatformIO().Viewports[i];
                if (vp->PlatformHandleRaw) {
                    HWND hwnd = (HWND)vp->PlatformHandleRaw;
                    BOOL use_dark = TRUE;
                    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &use_dark, sizeof(use_dark));
                }
            }
#endif
        }

        SDL_GL_SwapWindow(window);

        static bool window_shown = false;
        if (!window_shown) {
            SDL_ShowWindow(window);
            window_shown = true;
        }
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::DestroyPlatformWindows();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}