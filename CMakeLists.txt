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
set(VTERM_ENCODING_DIR "${libvterm_SOURCE_DIR}/src/encoding")
file(MAKE_DIRECTORY "${VTERM_ENCODING_DIR}")

if(NOT EXISTS "${VTERM_ENCODING_DIR}/DECdrawing.inc")
    file(WRITE "${VTERM_ENCODING_DIR}/DECdrawing.inc" "static const struct StaticTableEncoding encoding_DECdrawing = {\n  { .decode = &decode_table },\n  {\n    [0x60] = 0x25C6,\n    [0x61] = 0x2592,\n    [0x62] = 0x2409,\n    [0x63] = 0x240C,\n    [0x64] = 0x240D,\n    [0x65] = 0x240A,\n    [0x66] = 0x00B0,\n    [0x67] = 0x00B1,\n    [0x68] = 0x2424,\n    [0x69] = 0x240B,\n    [0x6a] = 0x2518,\n    [0x6b] = 0x2510,\n    [0x6c] = 0x250C,\n    [0x6d] = 0x2514,\n    [0x6e] = 0x253C,\n    [0x6f] = 0x23BA,\n    [0x70] = 0x23BB,\n    [0x71] = 0x2500,\n    [0x72] = 0x23BC,\n    [0x73] = 0x23BD,\n    [0x74] = 0x251C,\n    [0x75] = 0x2524,\n    [0x76] = 0x2534,\n    [0x77] = 0x252C,\n    [0x78] = 0x2502,\n    [0x79] = 0x2A7D,\n    [0x7a] = 0x2A7E,\n    [0x7b] = 0x03C0,\n    [0x7c] = 0x2260,\n    [0x7d] = 0x00A3,\n    [0x7e] = 0x00B7,\n  }\n};\n")
endif()

if(NOT EXISTS "${VTERM_ENCODING_DIR}/uk.inc")
    file(WRITE "${VTERM_ENCODING_DIR}/uk.inc" "static const struct StaticTableEncoding encoding_uk = {\n  { .decode = &decode_table },\n  {\n    [0x23] = 0x00a3,\n  }\n};\n")
endif()

file(GLOB VTERM_SOURCES ${libvterm_SOURCE_DIR}/src/*.c)
add_library(vterm STATIC ${VTERM_SOURCES})
target_include_directories(vterm PUBLIC ${libvterm_SOURCE_DIR}/include ${libvterm_SOURCE_DIR}/src)
target_compile_definitions(vterm PRIVATE -D_CRT_SECURE_NO_WARNINGS)

# FreeType — font rasterizer supporting color emojis and vector glyphs
FetchContent_Declare(
    freetype
    URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freetype)

# Dear ImGui (docking branch) — immediate-mode UI with dockable panels
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ImGui has no CMakeLists.txt, so we build it as a static library manually.
# This includes the core plus the SDL2+OpenGL3 backends and FreeType rasterizer.
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/freetype
)
target_compile_definitions(imgui_lib PUBLIC
    IMGUI_ENABLE_FREETYPE
    IMGUI_USE_WCHAR32
)
target_link_libraries(imgui_lib PUBLIC freetype SDL2::SDL2-static)

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

# ── Luce executable ───────────────────────────────────────────────────────────

file(GLOB_RECURSE LUCE_SOURCES CONFIGURE_DEPENDS src/*.cpp)

if(WIN32)
    list(APPEND LUCE_SOURCES luce.rc)
endif()

add_executable(luce ${LUCE_SOURCES})

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

# ── Copy assets, themes, and Lua plugins to output directory ──────────────────
add_custom_command(TARGET luce POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:luce>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:luce>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:luce>/themes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/themes"
            "$<TARGET_FILE_DIR:luce>/themes"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:luce>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins"
            "$<TARGET_FILE_DIR:luce>/plugins"
    COMMENT "Copying assets, themes, and plugins to output directory"
)
