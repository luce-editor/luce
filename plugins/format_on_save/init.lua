-- ============================================================================
-- format_on_save / init.lua — Auto-trim trailing whitespace before saving
-- ============================================================================

luce.plugin = {
    name        = "Format on Save",
    version     = "2.0.0",
    author      = "Luce Team",
    description = "Trims trailing whitespace on all lines before saving.",
}

luce.on("before_save", function(filepath)
    local line_count = luce.get_line_count()
    local modified = 0

    for i = 0, line_count - 1 do
        local line = luce.get_line(i)
        local trimmed = line:gsub("%s+$", "")
        if trimmed ~= line then
            luce.set_line(i, trimmed)
            modified = modified + 1
        end
    end

    if modified > 0 then
        luce.log("[Format on Save] Cleaned trailing spaces on " .. modified .. " line(s).")
    end
end)
