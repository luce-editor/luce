-- ============================================================================
-- uppercase.lua — Convert selected text to UPPERCASE
--
-- A simple example Luce plugin showing how to manipulate editor text.
-- Install: drop this file into the plugins/ folder next to luce.exe.
-- Use:     Ctrl+Shift+P → "UPPERCASE: Convert Selection"
-- ============================================================================

luce.plugin = {
    name        = "UPPERCASE Converter",
    version     = "1.0.0",
    author      = "Luce Team",
    description = "Converts the current text selection to UPPERCASE.",
}

local function uppercase_selection()
    local text = luce.get_selection()
    if text == "" then
        luce.set_status("UPPERCASE: No text selected.")
        return
    end
    luce.delete_selection()
    luce.insert_text(text:upper())
    luce.set_status("UPPERCASE: Converted " .. #text .. " characters.")
end

luce.register_command("uppercase", "UPPERCASE: Convert Selection", uppercase_selection)
