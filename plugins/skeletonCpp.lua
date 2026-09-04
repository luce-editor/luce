-- ============================================================================
--  skeletonCpp.lua -> insert basic c++ skeleton at cursor
--
--  Install: drop this file into plugins/ folder next to luce.exe
--  Use: Ctrl+Shift+P -> Skeleton: Insert basic Skeleton
-- ============================================================================

luce.plugin = {
    name = "Skeleton",
    version = "1.0.0",
    author = "Luce Team",
    description = "Inserts basic C++ skeleton"
}

local function insert_skel()
    local skelMesh = 
[[
#include <iostream>

int main(){
        
    return 0;
}
]]
    luce.insert_text(skelMesh)
    local file_name = luce.get_file_name()
    luce.show_notification(string.format("Skeleton inserted for %s", file_name), "success", 5)
end

local function insert_header_skel()
    local file_name = luce.get_file_name()
    local pos = string.find(file_name ,"%.")
    local class_name = pos and string.sub(file_name, 1, pos - 1) or file_name
    local skelMesh = string.format([[
#pragma once

class %s{
    public:
        %s();
        ~%s();
    private:

};
    ]], class_name, class_name, class_name)
    luce.insert_text(skelMesh)
    local file_name = luce.get_file_name()
    luce.show_notification(string.format("Header inserted for %s", file_name), "success", 5)
end

luce.register_command("insert_header_skel", "Skeleton: Insert Header Skeleton", insert_header_skel)
luce.register_command("insert_skeleton", "Skeleton: Insert Basic Skeleton", insert_skel)