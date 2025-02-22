load_background("bg_forest.png")
play_music("adventure.mp3")
show_text("You awaken in a mysterious forest. The air is thick with magic...", "Narrator", 60, 330)
set_choices({
    { text = "Follow the sound of water", scene = "scene_water.lua" },
    { text = "Walk into the dark woods", scene = "scene_dark.lua" }
})
