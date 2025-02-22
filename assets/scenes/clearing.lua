if last_scene == "scene_water.lua" then
    unload_sprite("Fairy")
    show_text("You have returned to the clearing...", "Narrator", 60, 330)
end
if last_scene == "main.lua" then
    load_background("bg_forest.png")
    play_music("adventure.mp3")
    show_text("You awaken in a mysterious forest. The air is thick with magic...", "Narrator", 60, 330)
end 
if last_scene == "scene_dark.lua" then
    load_background("bg_forest.png")
    show_text("You have returned to the clearing...", "Narrator", 60, 330)
end
set_choices({
    { text = "Follow the sound of water", scene = "scene_water.lua" },
    { text = "Walk into the dark woods", scene = "scene_dark.lua" }
})
