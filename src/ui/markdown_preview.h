#pragma once
// ============================================================================
// MarkdownPreview — Rich interactive preview widget for Markdown files.
// ============================================================================

#include "ui/theme.h"
#include "imgui.h"
#include <string>
#include <vector>

namespace luce {

class TextBuffer;

class MarkdownPreview {
public:
    MarkdownPreview();

    /// Render the markdown preview view
    void Render(const char* id, const TextBuffer* buffer, const Theme& theme,
                ImFont* bold_font, ImFont* italic_font, ImFont* h1_font, ImFont* h2_font);

private:
    void RenderMarkdownLine(const std::string& line, const Theme& theme,
                            ImFont* bold_font, ImFont* italic_font, ImFont* h1_font, ImFont* h2_font);
};

}  // namespace luce
