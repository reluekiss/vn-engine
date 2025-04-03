Narrator = { name = "Narrator" }
local scene_map = {
    ["scene_water.lua"] = function()
        unload_sprite("Fairy")
        show_text(Narrator, "You have returned to the clearing...")
    end,
    ["test_main.lua"] = function()
        load_background("bg_forest.png")
        play_music("adventure.mp3")
        show_text(Narrator, "You awaken in a mysterious forest. The air is thick with magic...")
    end,
    ["scene_dark.lua"] = function()
        load_background("bg_forest.png")
        play_music("adventure.mp3")
        show_text(Narrator, "You have returned to the clearing...")
    end
}

if scene_map[last_scene] then
    scene_map[last_scene]()
end

set_choices({
    { text = "Follow the sound of water", scene = "scene_water.lua" },
    { text = "Walk into the dark woods", scene = "scene_dark.lua" }
})
