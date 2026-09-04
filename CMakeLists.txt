cmake_minimum_required(VERSION 3.20)
project(Luce VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies via FetchContent ──────────────────────────────────────────────

include(FetchContent)

# Lua 5.4 — embedded scripting engine for Lua plugins
FetchContent_Declare(
    lua
    URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)

# Build Lua as a static library (excluding the standalone interpreter/compiler).
file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "(lua|luac)\.c$")
add_library(lua_lib STATIC ${LUA_SOURCES})
target_include_directories(lua_lib PUBLIC "${lua_SOURCE_DIR}/src")
if(UNIX)
    target_compile_definitions(lua_lib PUBLIC LUA_USE_POSIX)
endif()

# SDL2 — windowing and input
FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.x
    GIT_SHALLOW    TRUE
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

# libvterm — terminal emulator library used by Neovim
FetchContent_Declare(
    libvterm
    GIT_REPOSITORY https://github.com/neovim/libvterm.git
    GIT_TAG        v0.3.3
    GIT_SHALLOW    TRUE
)
set(VTERM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VTERM_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libvterm)

# libvterm doesn't have a CMakeLists.txt, so we create a static library for it manually
file(GLOB VTERM_SOURCES ${libvterm_SOURCE_DIR}/src/*.c)
add_library(vterm STATIC ${VTERM_SOURCES})
target_include_directories(vterm PUBLIC ${libvterm_SOURCE_DIR}/include)
target_compile_definitions(vterm PRIVATE -D_CRT_SECURE_NO_WARNINGS)

# Dear ImGui (docking branch) — immediate-mode UI with dockable panels
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ImGui has no CMakeLists.txt, so we build it as a static library manually.
# This includes the core plus the SDL2+OpenGL3 backends.
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2-static)

# OpenGL — system-provided
find_package(OpenGL REQUIRED)

# ── Download JetBrains Mono (OFL licensed) at configure time ───────────────────

set(FONT_DIR "${CMAKE_BINARY_DIR}/fonts")
file(MAKE_DIRECTORY "${FONT_DIR}")

set(JETBRAINS_MONO_URL
    "https://github.com/JetBrains/JetBrainsMono/raw/master/fonts/ttf/JetBrainsMono-Regular.ttf")
set(JETBRAINS_MONO_PATH "${FONT_DIR}/JetBrainsMono-Regular.ttf")

if(NOT EXISTS "${JETBRAINS_MONO_PATH}")
    message(STATUS "Downloading JetBrains Mono font...")
    file(DOWNLOAD "${JETBRAINS_MONO_URL}" "${JETBRAINS_MONO_PATH}"
         STATUS DOWNLOAD_STATUS
         TIMEOUT 30)
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        message(WARNING "Failed to download JetBrains Mono. The editor will use the built-in font.")
    endif()
endif()

# Copy assets (fonts and icons) and themes next to the executable so it can be found at runtime
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/assets_copied"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:luce>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/themes"
            "$<TARGET_FILE_DIR:luce>/themes"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/assets_copied"
    COMMENT "Copying assets and themes to output directory"
)

# ── Luce executable ───────────────────────────────────────────────────────────

file(GLOB_RECURSE LUCE_SOURCES CONFIGURE_DEPENDS src/*.cpp)

if(WIN32)
    list(APPEND LUCE_SOURCES luce.rc)
endif()

add_executable(luce ${LUCE_SOURCES})
add_custom_target(copy_assets ALL DEPENDS "${CMAKE_BINARY_DIR}/assets_copied")
add_dependencies(luce copy_assets)

target_include_directories(luce PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(luce PRIVATE
    imgui_lib
    lua_lib
    vterm
    OpenGL::GL
    SDL2::SDL2-static
    SDL2::SDL2main
)

target_include_directories(luce PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${lua_SOURCE_DIR}
    ${libvterm_SOURCE_DIR}/include
)

# Pass the font directory path to C++ as a compile definition
target_compile_definitions(luce PRIVATE
    LUCE_VERSION="${PROJECT_VERSION}"
    LUCE_FONT_DIR="fonts"
)

# Windows: link shell32 for file dialogs, suppress console in Release
if(WIN32)
    target_link_libraries(luce PRIVATE shell32 comdlg32 ole32 uuid dwmapi)
    set_target_properties(luce PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()

# ── Copy Lua plugins to output directory ──────────────────────────────────────
add_custom_command(TARGET luce POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:luce>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins"
            "$<TARGET_FILE_DIR:luce>/plugins"
    COMMENT "Copying Lua plugins to output directory"
)
