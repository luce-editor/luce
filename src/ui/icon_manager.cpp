// ============================================================================
// IconManager — Implementation.
// ============================================================================

#include "icon_manager.h"
#include "platform.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#include <algorithm>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace luce {

IconManager::~IconManager() {
    for (auto& [path, tex] : textures_) {
        if (tex != 0) {
            glDeleteTextures(1, &tex);
        }
    }
    textures_.clear();
}

void IconManager::Init(const std::string& icons_directory) {
    icons_dir_ = icons_directory;
    std::ranges::replace(icons_dir_, '\\', '/');

    // Extension -> SVG filename mapping
    ext_to_svg_[".cpp"] = "file_type_cpp.svg";
    ext_to_svg_[".cxx"] = "file_type_cpp.svg";
    ext_to_svg_[".cc"]  = "file_type_cpp.svg";
    ext_to_svg_[".c"]   = "file_type_c.svg";
    ext_to_svg_[".h"]   = "file_type_cheader.svg";
    ext_to_svg_[".hpp"] = "file_type_cppheader.svg";
    ext_to_svg_[".hxx"] = "file_type_cppheader.svg";
    ext_to_svg_[".rs"]  = "file_type_rust.svg";
    ext_to_svg_[".py"]  = "file_type_python.svg";
    ext_to_svg_[".pyw"] = "file_type_python.svg";
    ext_to_svg_[".html"] = "file_type_html.svg";
    ext_to_svg_[".htm"]  = "file_type_html.svg";
    ext_to_svg_[".css"]  = "file_type_css.svg";
    ext_to_svg_[".scss"] = "file_type_scss.svg";
    ext_to_svg_[".less"] = "file_type_less.svg";
    ext_to_svg_[".js"]   = "file_type_js.svg";
    ext_to_svg_[".mjs"]  = "file_type_js.svg";
    ext_to_svg_[".jsx"]  = "file_type_reactjs.svg";
    ext_to_svg_[".ts"]   = "file_type_typescript.svg";
    ext_to_svg_[".tsx"]  = "file_type_reactts.svg";
    ext_to_svg_[".json"] = "file_type_json.svg";
    ext_to_svg_[".md"]   = "file_type_markdown.svg";
    ext_to_svg_[".markdown"] = "file_type_markdown.svg";
    ext_to_svg_[".txt"]  = "default_file.svg";
    ext_to_svg_[".log"]  = "file_type_log.svg";
    ext_to_svg_[".toml"] = "file_type_toml.svg";
    ext_to_svg_[".yaml"] = "file_type_yaml.svg";
    ext_to_svg_[".yml"]  = "file_type_yaml.svg";
    ext_to_svg_[".ini"]  = "file_type_ini.svg";
    ext_to_svg_[".svg"]  = "file_type_image.svg";
    ext_to_svg_[".png"]  = "file_type_image.svg";
    ext_to_svg_[".jpg"]  = "file_type_image.svg";
    ext_to_svg_[".jpeg"] = "file_type_image.svg";
    ext_to_svg_[".ttf"]  = "file_type_font.svg";
    ext_to_svg_[".otf"]  = "file_type_font.svg";
    ext_to_svg_[".woff"] = "file_type_font.svg";
    ext_to_svg_[".cmake"] = "file_type_cmake.svg";
}

ImTextureID IconManager::GetTexture(const std::string& svg_filename, bool fallback_to_default) {
    if (icons_dir_.empty()) return 0;

    std::string full_path = icons_dir_ + "/" + svg_filename;
    if (!fs::exists(full_path)) {
        if (!fallback_to_default) return 0;
        full_path = icons_dir_ + "/default_file.svg";
        if (!fs::exists(full_path)) return 0;
    }

    auto it = textures_.find(full_path);
    if (it != textures_.end()) {
        return (ImTextureID)(intptr_t)it->second;
    }

    // Parse at high DPI so internal SVG units map to more pixels
    NSVGimage* image = nsvgParseFromFile(full_path.c_str(), "px", 192.0f);
    if (!image) return 0;

    // Rasterize at 256x256 for maximum crispness on any display scale
    int width = 256;
    int height = 256;
    float svg_w = image->width > 0 ? image->width : 32.0f;
    float svg_h = image->height > 0 ? image->height : 32.0f;
    float scale_x = (float)width  / svg_w;
    float scale_y = (float)height / svg_h;
    float scale   = scale_x < scale_y ? scale_x : scale_y;

    // Center the SVG in the output if aspect ratio differs
    float tx = ((float)width  - svg_w * scale) * 0.5f;
    float ty = ((float)height - svg_h * scale) * 0.5f;

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) {
        nsvgDelete(image);
        return 0;
    }

    std::vector<unsigned char> img_data(width * height * 4, 0);
    nsvgRasterize(rast, image, tx, ty, scale, img_data.data(), width, height, width * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    // Create OpenGL texture with linear filtering
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); // GL_CLAMP_TO_EDGE
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    textures_[full_path] = tex;
    return (ImTextureID)(intptr_t)tex;
}

ImTextureID IconManager::GetIconForFile(const std::string& filename) {
    if (filename == "CMakeLists.txt") return GetTexture("file_type_cmake.svg");
    if (filename == ".gitignore")     return GetTexture("file_type_git.svg");
    if (filename == "LICENSE")        return GetTexture("file_type_license.svg");
    if (filename == "README.md")      return GetTexture("file_type_markdown.svg");

    std::string ext = platform::GetExtension(filename);
    std::ranges::transform(ext, ext.begin(), ::tolower);

    auto it = ext_to_svg_.find(ext);
    if (it != ext_to_svg_.end()) {
        return GetTexture(it->second);
    }
    return GetTexture("default_file.svg");
}

ImTextureID IconManager::GetFolderIcon(bool is_open) {
    return GetTexture(is_open ? "default_folder_opened.svg" : "default_folder.svg");
}

ImTextureID IconManager::GetIconByName(const std::string& name) {
    return GetTexture(name + ".svg", false);
}

}  // namespace luce
