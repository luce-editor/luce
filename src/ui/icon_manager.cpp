// ============================================================================
// IconManager — Implementation.
// Supports built-in assets/icons, user custom icons directory, and icons.json mappings.
// ============================================================================

#include "icon_manager.h"
#include "platform.h"
#include "external/json.hpp"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#include "stb_image.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace luce {

IconManager::~IconManager() {
    for (auto& [path, tex] : textures_) {
        if (tex != 0) {
            glDeleteTextures(1, &tex);
        }
    }
    textures_.clear();
}

void IconManager::Init(const std::string& default_icons_directory,
                       const std::string& custom_icons_directory) {
    default_icons_dir_ = default_icons_directory;
    std::ranges::replace(default_icons_dir_, '\\', '/');

    custom_icons_dir_ = custom_icons_directory;
    std::ranges::replace(custom_icons_dir_, '\\', '/');

    std::string exe_dir = platform::GetExecutableDir();
    std::ranges::replace(exe_dir, '\\', '/');

    if (custom_icons_dir_.empty()) {
        std::vector<std::string> custom_candidates = {
            exe_dir + "/icons",
            exe_dir + "/../icons",
            "icons"
        };
        for (const auto& c : custom_candidates) {
            if (fs::exists(c)) {
                custom_icons_dir_ = c;
                break;
            }
        }
        if (custom_icons_dir_.empty()) {
            custom_icons_dir_ = exe_dir + "/icons";
        }
    }

    // Build search directories: custom user icons take priority over built-ins!
    search_dirs_.clear();
    if (!custom_icons_dir_.empty() && fs::exists(custom_icons_dir_)) {
        search_dirs_.push_back(custom_icons_dir_);
    }

    if (!default_icons_dir_.empty() && fs::exists(default_icons_dir_)) {
        search_dirs_.push_back(default_icons_dir_);
    }

    // Load default mappings and then overlay with icons.json
    LoadDefaultMappings();

    std::string config_path = GetConfigPath();
    if (!config_path.empty() && fs::exists(config_path)) {
        LoadConfigFile(config_path);
    }
}

void IconManager::LoadDefaultMappings() {
    ext_to_svg_.clear();
    filename_to_svg_.clear();
    default_folder_svg_      = "default_folder.svg";
    default_folder_open_svg_ = "default_folder_opened.svg";

    // Common exact filenames
    filename_to_svg_["CMakeLists.txt"]     = "file_type_cmake.svg";
    filename_to_svg_[".gitignore"]         = "file_type_git.svg";
    filename_to_svg_[".gitmodules"]        = "file_type_git.svg";
    filename_to_svg_[".gitattributes"]     = "file_type_git.svg";
    filename_to_svg_["LICENSE"]            = "file_type_license.svg";
    filename_to_svg_["LICENSE.md"]         = "file_type_license.svg";
    filename_to_svg_["LICENSE.txt"]        = "file_type_license.svg";
    filename_to_svg_["README.md"]          = "file_type_markdown.svg";
    filename_to_svg_["Dockerfile"]         = "file_type_docker.svg";
    filename_to_svg_["docker-compose.yml"] = "file_type_docker.svg";
    filename_to_svg_["docker-compose.yaml"]= "file_type_docker.svg";
    filename_to_svg_["Makefile"]           = "file_type_makefile.svg";
    filename_to_svg_["package.json"]       = "file_type_npm.svg";
    filename_to_svg_["package-lock.json"]  = "file_type_npm.svg";
    filename_to_svg_["Cargo.toml"]         = "file_type_cargo.svg";
    filename_to_svg_["Cargo.lock"]         = "file_type_cargo.svg";
    filename_to_svg_["go.mod"]             = "file_type_go.svg";
    filename_to_svg_["go.sum"]             = "file_type_go.svg";
    filename_to_svg_[".editorconfig"]      = "file_type_editorconfig.svg";
    filename_to_svg_["tsconfig.json"]      = "file_type_tsconfig.svg";
    filename_to_svg_["jsconfig.json"]      = "file_type_jsconfig.svg";
    filename_to_svg_["settings.json"]      = "file_type_settings.svg";

    // C / C++
    ext_to_svg_[".cpp"]   = "file_type_cpp.svg";
    ext_to_svg_[".cxx"]   = "file_type_cpp.svg";
    ext_to_svg_[".cc"]    = "file_type_cpp.svg";
    ext_to_svg_[".c"]     = "file_type_c.svg";
    ext_to_svg_[".h"]     = "file_type_cheader.svg";
    ext_to_svg_[".hpp"]   = "file_type_cppheader.svg";
    ext_to_svg_[".hxx"]   = "file_type_cppheader.svg";

    // Modern Systems & General Purpose
    ext_to_svg_[".rs"]    = "file_type_rust.svg";
    ext_to_svg_[".go"]    = "file_type_go.svg";
    ext_to_svg_[".zig"]   = "file_type_zig.svg";
    ext_to_svg_[".odin"]  = "file_type_odin.svg";
    ext_to_svg_[".v"]     = "file_type_v.svg";
    ext_to_svg_[".swift"] = "file_type_swift.svg";
    ext_to_svg_[".d"]     = "file_type_dlang.svg";
    ext_to_svg_[".nim"]   = "file_type_nim.svg";

    // Scripting & VM Languages
    ext_to_svg_[".py"]    = "file_type_python.svg";
    ext_to_svg_[".pyw"]   = "file_type_python.svg";
    ext_to_svg_[".lua"]   = "file_type_lua.svg";
    ext_to_svg_[".luau"]  = "file_type_luau.svg";
    ext_to_svg_[".rb"]    = "file_type_ruby.svg";
    ext_to_svg_[".php"]   = "file_type_php.svg";
    ext_to_svg_[".java"]  = "file_type_java.svg";
    ext_to_svg_[".jar"]   = "file_type_jar.svg";
    ext_to_svg_[".class"] = "file_type_class.svg";
    ext_to_svg_[".kt"]    = "file_type_kotlin.svg";
    ext_to_svg_[".kts"]   = "file_type_kotlin.svg";
    ext_to_svg_[".cs"]    = "file_type_csharp.svg";
    ext_to_svg_[".fs"]    = "file_type_fsharp.svg";
    ext_to_svg_[".dart"]  = "file_type_dartlang.svg";
    ext_to_svg_[".scala"] = "file_type_scala.svg";
    ext_to_svg_[".hs"]    = "file_type_haskell.svg";
    ext_to_svg_[".clj"]   = "file_type_clojure.svg";
    ext_to_svg_[".ex"]    = "file_type_elixir.svg";
    ext_to_svg_[".exs"]   = "file_type_elixir.svg";
    ext_to_svg_[".erl"]   = "file_type_erlang.svg";
    ext_to_svg_[".r"]     = "file_type_r.svg";
    ext_to_svg_[".jl"]    = "file_type_julia.svg";

    // Web & Frontend
    ext_to_svg_[".html"]  = "file_type_html.svg";
    ext_to_svg_[".htm"]   = "file_type_html.svg";
    ext_to_svg_[".css"]   = "file_type_css.svg";
    ext_to_svg_[".scss"]  = "file_type_scss.svg";
    ext_to_svg_[".less"]  = "file_type_less.svg";
    ext_to_svg_[".js"]    = "file_type_js.svg";
    ext_to_svg_[".mjs"]   = "file_type_js.svg";
    ext_to_svg_[".cjs"]   = "file_type_js.svg";
    ext_to_svg_[".jsx"]   = "file_type_reactjs.svg";
    ext_to_svg_[".ts"]    = "file_type_typescript.svg";
    ext_to_svg_[".tsx"]   = "file_type_reactts.svg";
    ext_to_svg_[".vue"]   = "file_type_vue.svg";
    ext_to_svg_[".svelte"]= "file_type_svelte.svg";
    ext_to_svg_[".astro"] = "file_type_astro.svg";

    // Data & Config Formats
    ext_to_svg_[".json"]  = "file_type_json.svg";
    ext_to_svg_[".json5"] = "file_type_json5.svg";
    ext_to_svg_[".toml"]  = "file_type_toml.svg";
    ext_to_svg_[".yaml"]  = "file_type_yaml.svg";
    ext_to_svg_[".yml"]   = "file_type_yaml.svg";
    ext_to_svg_[".ini"]   = "file_type_ini.svg";
    ext_to_svg_[".xml"]   = "file_type_xml.svg";
    ext_to_svg_[".csv"]   = "file_type_csv.svg";
    ext_to_svg_[".sql"]   = "file_type_sql.svg";
    ext_to_svg_[".graphql"] = "file_type_graphql.svg";
    ext_to_svg_[".gql"]   = "file_type_graphql.svg";
    ext_to_svg_[".proto"] = "file_type_protobuf.svg";

    // Shell & Scripts
    ext_to_svg_[".sh"]    = "file_type_shell.svg";
    ext_to_svg_[".bash"]  = "file_type_shell.svg";
    ext_to_svg_[".zsh"]   = "file_type_shell.svg";
    ext_to_svg_[".bat"]   = "file_type_bat.svg";
    ext_to_svg_[".cmd"]   = "file_type_bat.svg";
    ext_to_svg_[".ps1"]   = "file_type_powershell.svg";
    ext_to_svg_[".psm1"]  = "file_type_powershell.svg";
    ext_to_svg_[".psd1"]  = "file_type_powershell.svg";

    // Shaders & Graphics
    ext_to_svg_[".glsl"]  = "file_type_glsl.svg";
    ext_to_svg_[".vert"]  = "file_type_glsl.svg";
    ext_to_svg_[".frag"]  = "file_type_glsl.svg";
    ext_to_svg_[".geom"]  = "file_type_glsl.svg";
    ext_to_svg_[".hlsl"]  = "file_type_hlsl.svg";

    // Text & Documents
    ext_to_svg_[".md"]    = "file_type_markdown.svg";
    ext_to_svg_[".markdown"] = "file_type_markdown.svg";
    ext_to_svg_[".txt"]   = "default_file.svg";
    ext_to_svg_[".log"]   = "file_type_log.svg";
    ext_to_svg_[".pdf"]   = "file_type_pdf.svg";
    ext_to_svg_[".diff"]  = "file_type_diff.svg";
    ext_to_svg_[".patch"] = "file_type_patch.svg";

    // Assets & Media
    ext_to_svg_[".svg"]   = "file_type_image.svg";
    ext_to_svg_[".png"]   = "file_type_image.svg";
    ext_to_svg_[".jpg"]   = "file_type_image.svg";
    ext_to_svg_[".jpeg"]  = "file_type_image.svg";
    ext_to_svg_[".gif"]   = "file_type_image.svg";
    ext_to_svg_[".webp"]  = "file_type_image.svg";
    ext_to_svg_[".ico"]   = "file_type_favicon.svg";
    ext_to_svg_[".ttf"]   = "file_type_font.svg";
    ext_to_svg_[".otf"]   = "file_type_font.svg";
    ext_to_svg_[".woff"]  = "file_type_font.svg";
    ext_to_svg_[".woff2"] = "file_type_font.svg";

    // Build Systems & Archives
    ext_to_svg_[".cmake"] = "file_type_cmake.svg";
    ext_to_svg_[".zip"]   = "file_type_zip.svg";
    ext_to_svg_[".tar"]   = "file_type_zip.svg";
    ext_to_svg_[".gz"]    = "file_type_zip.svg";
    ext_to_svg_[".7z"]    = "file_type_zip.svg";
    ext_to_svg_[".rar"]   = "file_type_zip.svg";
}

void IconManager::LoadConfigFile(const std::string& config_path) {
    if (!fs::exists(config_path)) return;

    std::ifstream f(config_path);
    if (!f.is_open()) return;

    try {
        json j = json::parse(f, nullptr, true, true); // Allow comments

        if (j.contains("extensions") && j["extensions"].is_object()) {
            for (auto& [ext, icon] : j["extensions"].items()) {
                if (icon.is_string()) {
                    std::string e = ext;
                    if (!e.empty() && e[0] != '.') e = "." + e;
                    std::ranges::transform(e, e.begin(), ::tolower);
                    ext_to_svg_[e] = icon.get<std::string>();
                }
            }
        }

        if (j.contains("filenames") && j["filenames"].is_object()) {
            for (auto& [fname, icon] : j["filenames"].items()) {
                if (icon.is_string()) {
                    filename_to_svg_[fname] = icon.get<std::string>();
                }
            }
        }

        if (j.contains("folders") && j["folders"].is_object()) {
            auto& fld = j["folders"];
            if (fld.contains("default") && fld["default"].is_string()) {
                default_folder_svg_ = fld["default"].get<std::string>();
            }
            if (fld.contains("default_open") && fld["default_open"].is_string()) {
                default_folder_open_svg_ = fld["default_open"].get<std::string>();
            }
        }
    } catch (...) {
        // Parse error ignored
    }
}

std::string IconManager::GetConfigPath() const {
    if (!custom_icons_dir_.empty()) {
        std::string p = custom_icons_dir_ + "/icons.json";
        if (fs::exists(p)) return p;
    }

    std::string exe_dir = platform::GetExecutableDir();
    std::ranges::replace(exe_dir, '\\', '/');

    std::vector<std::string> candidates = {
        exe_dir + "/icons/icons.json",
        "icons/icons.json"
    };

    for (const auto& c : candidates) {
        if (fs::exists(c)) return c;
    }

    if (!custom_icons_dir_.empty()) {
        return custom_icons_dir_ + "/icons.json";
    }
    return exe_dir + "/icons/icons.json";
}

void IconManager::Reload() {
    // Delete all OpenGL textures
    for (auto& [path, tex] : textures_) {
        if (tex != 0) {
            glDeleteTextures(1, &tex);
        }
    }
    textures_.clear();

    // Re-initialize directories and mappings
    Init(default_icons_dir_, custom_icons_dir_);
}

std::string IconManager::FindIconFile(const std::string& icon_filename) const {
    if (icon_filename.empty()) return "";

    // If it's already an absolute or relative valid path
    if (fs::exists(icon_filename)) {
        return icon_filename;
    }

    // Search through directories (custom user icons directory first!)
    for (const auto& dir : search_dirs_) {
        std::string candidate = dir + "/" + icon_filename;
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    return "";
}

ImTextureID IconManager::GetTexture(const std::string& path_or_filename, bool fallback_to_default) {
    if (path_or_filename.empty()) return 0;

    std::string full_path = FindIconFile(path_or_filename);

    if (full_path.empty()) {
        if (!fallback_to_default) return 0;
        full_path = FindIconFile("default_file.svg");
        if (full_path.empty()) return 0;
    }

    std::ranges::replace(full_path, '\\', '/');

    auto it = textures_.find(full_path);
    if (it != textures_.end()) {
        return (ImTextureID)(intptr_t)it->second;
    }

    // Check file extension: PNG/JPG or SVG?
    std::string ext = platform::GetExtension(full_path);
    std::ranges::transform(ext, ext.begin(), ::tolower);

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
        int w = 0, h = 0, ch = 0;
        unsigned char* data = stbi_load(full_path.c_str(), &w, &h, &ch, 4);
        if (!data) return 0;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);

        textures_[full_path] = tex;
        return (ImTextureID)(intptr_t)tex;
    }

    // Parse SVG at high DPI
    NSVGimage* image = nsvgParseFromFile(full_path.c_str(), "px", 192.0f);
    if (!image) return 0;

    // Rasterize at 256x256 for crisp rendering
    int width = 256;
    int height = 256;
    float svg_w = image->width > 0 ? image->width : 32.0f;
    float svg_h = image->height > 0 ? image->height : 32.0f;
    float scale_x = (float)width  / svg_w;
    float scale_y = (float)height / svg_h;
    float scale   = scale_x < scale_y ? scale_x : scale_y;

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
    // 1. Check exact filename match
    auto it_fn = filename_to_svg_.find(filename);
    if (it_fn != filename_to_svg_.end()) {
        ImTextureID t = GetTexture(it_fn->second, false);
        if (t) return t;
    }

    // 2. Check extension match
    std::string ext = platform::GetExtension(filename);
    std::ranges::transform(ext, ext.begin(), ::tolower);

    auto it_ext = ext_to_svg_.find(ext);
    if (it_ext != ext_to_svg_.end()) {
        ImTextureID t = GetTexture(it_ext->second, false);
        if (t) return t;
    }

    // 3. Auto-fallback discovery: check file_type_<ext>.svg or <ext>.svg
    if (!ext.empty()) {
        std::string raw_ext = (ext[0] == '.') ? ext.substr(1) : ext;
        ImTextureID t = GetTexture("file_type_" + raw_ext + ".svg", false);
        if (t) return t;
        t = GetTexture(raw_ext + ".svg", false);
        if (t) return t;
        t = GetTexture(raw_ext + ".png", false);
        if (t) return t;
    }

    return GetTexture("default_file.svg");
}

ImTextureID IconManager::GetFolderIcon(bool is_open) {
    return GetTexture(is_open ? default_folder_open_svg_ : default_folder_svg_);
}

ImTextureID IconManager::GetIconByName(const std::string& name) {
    if (name.empty()) return 0;

    if (name.ends_with(".svg") || name.ends_with(".png") || name.ends_with(".jpg")) {
        ImTextureID t = GetTexture(name, false);
        if (t) return t;
    }

    ImTextureID t = GetTexture(name + ".svg", false);
    if (t) return t;
    t = GetTexture(name + ".png", false);
    if (t) return t;
    t = GetTexture("file_type_" + name + ".svg", false);
    if (t) return t;
    t = GetTexture("folder_type_" + name + ".svg", false);
    if (t) return t;
    t = GetTexture("default_" + name + ".svg", false);
    if (t) return t;

    return 0;
}

}  // namespace luce
