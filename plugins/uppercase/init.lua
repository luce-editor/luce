-- ============================================================================
-- uppercase / init.lua — Convert selected text to UPPERCASE
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
