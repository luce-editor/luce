-- ============================================================================
-- timestamp.lua - Insert current ISO 8601 timestamp at cursor
--
-- Install: drop this file into the plugins/ folder next to luce.exe.
-- Use:     Ctrl+Shift+P -> "Timestamp: Insert at Cursor"
-- ============================================================================

luce.plugin = {
    name        = "Timestamp Inserter",
    version     = "1.0.0",
    author      = "Luce Team",
    description = "Inserts the current UTC date/time as a code comment.",
}

local function insert_timestamp()
    -- os.date uses the system clock; %z gives the UTC offset.
    local ts = os.date("!%Y-%m-%dT%H:%M:%SZ")
    luce.insert_text("// " .. ts .. "\n")
    luce.set_status("Timestamp: inserted " .. ts)
end

local function insert_date_only()
    local d = os.date("!%Y-%m-%d")
    luce.insert_text(d)
    luce.set_status("Date inserted: " .. d)
end

luce.register_command("insert_timestamp", "Timestamp: Insert Date & Time",  insert_timestamp)
luce.register_command("insert_date",      "Timestamp: Insert Date Only",     insert_date_only)
