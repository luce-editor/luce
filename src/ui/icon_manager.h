#pragma once
// ============================================================================
// IconManager — Loads, caches and rasters SVG and PNG icons directly into OpenGL textures.
// Supports custom user icons, custom search directories, and icons.json mappings.
// ============================================================================

#include "imgui.h"
#include <string>
#include <unordered_map>
#include <vector>

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

    /// Initialise with default icons directory and optional custom icons directory.
    void Init(const std::string& default_icons_directory,
              const std::string& custom_icons_directory = "");

    /// Reload all icons, clear OpenGL texture cache, and re-read icons.json.
    void Reload();

    /// Get path to custom icons directory.
    const std::string& GetCustomIconsDir() const { return custom_icons_dir_; }

    /// Get path to the default icons directory (assets/icons).
    const std::string& GetDefaultIconsDir() const { return default_icons_dir_; }

    /// Get path to active icons.json configuration file.
    std::string GetConfigPath() const;

    /// Get OpenGL texture ID for a specific file (by exact filename or extension).
    ImTextureID GetIconForFile(const std::string& filename);

    /// Get OpenGL texture ID for folder (closed or open).
    ImTextureID GetFolderIcon(bool is_open);

    /// Get OpenGL texture ID for an icon by name (e.g. "delete", "refresh", "file_type_cpp").
    ImTextureID GetIconByName(const std::string& name);

    /// Load or retrieve cached texture for an icon name or file path.
    ImTextureID GetTexture(const std::string& path_or_filename, bool fallback_to_default = true);

private:
    IconManager() = default;
    ~IconManager();

    void LoadDefaultMappings();
    void LoadConfigFile(const std::string& config_path);
    std::string FindIconFile(const std::string& filename) const;

    std::string default_icons_dir_;
    std::string custom_icons_dir_;
    std::vector<std::string> search_dirs_;

    std::unordered_map<std::string, unsigned int> textures_; // full_path -> GLuint
    std::unordered_map<std::string, std::string> ext_to_svg_;
    std::unordered_map<std::string, std::string> filename_to_svg_;

    std::string default_folder_svg_      = "default_folder.svg";
    std::string default_folder_open_svg_ = "default_folder_opened.svg";
};

}  // namespace luce
