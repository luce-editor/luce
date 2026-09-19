-- ============================================================================
-- json_intellisense/init.lua — JSON IntelliSense & Schema Completion
--
-- Provides smart JSON autocompletion:
--   - Keywords and primitives: true, false, null
--   - Settings keys for settings.json: editor, ui, font_size, tab_size,
--     use_spaces, show_minimap, show_line_numbers, highlight_current_line,
--     zoom_with_mouse_wheel, cursor_blinking, scale, theme, auto_save
--   - Built-in theme name autocompletions: "VS Code Dark 2026", "Catppuccin Mocha",
--     "One Dark", "Nord", "Dracula"
-- ============================================================================

luce.plugin = {
    name        = "JSON IntelliSense & Schema",
    version     = "1.0.0",
    author      = "Luce Team",
    description = "Provides autocompletion and schema suggestions for JSON files and settings.json.",
}

local json_primitives = {
    "true",
    "false",
    "null",
}

local settings_editor_keys = {
    '"font_size"',
    '"tab_size"',
    '"use_spaces"',
    '"show_minimap"',
    '"show_line_numbers"',
    '"highlight_current_line"',
    '"zoom_with_mouse_wheel"',
    '"cursor_blinking"',
}

local settings_ui_keys = {
    '"font_size"',
    '"scale"',
    '"theme"',
    '"auto_save"',
}

local settings_root_keys = {
    '"$schema"',
    '"editor"',
    '"ui"',
}

local theme_values = {
    '"VS Code Dark 2026"',
    '"Catppuccin Mocha"',
    '"One Dark"',
    '"Nord"',
    '"Dracula"',
}

local auto_save_values = {
    '"off"',
    '"on_focus_lost"',
    '"after_delay"',
}

luce.register_completion_provider(".json", function(prefix, line, col)
    local results = {}
    local lower_prefix = prefix:lower()

    local function add_if_matches(item)
        if item:lower():find(lower_prefix, 1, true) then
            -- Avoid duplicates
            for _, existing in ipairs(results) do
                if existing == item then return end
            end
            table.insert(results, item)
        end
    end

    -- Add primitives
    for _, item in ipairs(json_primitives) do
        add_if_matches(item)
    end

    -- Add root settings keys
    for _, item in ipairs(settings_root_keys) do
        add_if_matches(item)
    end

    -- Add editor and ui keys
    for _, item in ipairs(settings_editor_keys) do
        add_if_matches(item)
    end
    for _, item in ipairs(settings_ui_keys) do
        add_if_matches(item)
    end

    -- Add theme options
    for _, item in ipairs(theme_values) do
        add_if_matches(item)
    end

    -- Add auto_save options
    for _, item in ipairs(auto_save_values) do
        add_if_matches(item)
    end

    return results
end)
