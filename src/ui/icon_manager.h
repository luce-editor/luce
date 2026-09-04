#pragma once
// ============================================================================
// IconManager — Loads, caches and rasters SVG icons directly into OpenGL textures.
// ============================================================================

#include "imgui.h"
#include <string>
#include <unordered_map>

#if defined(_WIN32)
#   include <windows.h>
#   include <GL/gl.h>
#else
#   include <GL/gl.h>
#endif

namespace luce {

class IconManager {
public:
    static IconManager& Instance() {
        static IconManager instance;
        return instance;
    }

    /// Initialise with search directories for SVG icons
    void Init(const std::string& icons_directory);

    /// Get OpenGL texture ID for a specific icon by file extension or name
    ImTextureID GetIconForFile(const std::string& filename);
    ImTextureID GetFolderIcon(bool is_open);
    ImTextureID GetIconByName(const std::string& name);

    /// Load or retrieve cached texture for an SVG file path
    ImTextureID GetTexture(const std::string& svg_path);

private:
    IconManager() = default;
    ~IconManager();

    std::string icons_dir_;
    std::unordered_map<std::string, unsigned int> textures_; // svg_path -> GLuint
    std::unordered_map<std::string, std::string> ext_to_svg_;
};

}  // namespace luce
