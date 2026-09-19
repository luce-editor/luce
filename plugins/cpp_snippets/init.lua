-- ============================================================================
-- cpp_snippets/init.lua — C++ Snippets & Completion Provider
--
-- Demonstrates directory-based plugins and 'register_completion_provider'.
-- ============================================================================

luce.plugin = {
    name        = "C++ Snippets & Helpers",
    version     = "1.0.0",
    author      = "Luce Team",
    description = "Provides smart C++ autocompletion snippets.",
}

local snippets = {
    "std::cout",
    "std::vector",
    "std::string",
    "std::unique_ptr",
    "std::shared_ptr",
    "std::make_unique",
    "std::make_shared",
    "std::optional",
    "std::format",
    "std::views",
    "std::ranges",
    "constexpr",
    "consteval",
    "noexcept",
    "override",
    "nodiscard",
}

luce.register_completion_provider(".cpp", function(prefix, line, col)
    local results = {}
    local lower_prefix = prefix:lower()
    for _, item in ipairs(snippets) do
        if item:lower():find(lower_prefix, 1, true) then
            table.insert(results, item)
        end
    end
    return results
end)
