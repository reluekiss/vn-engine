load_sprite("fairy.png", 200, 150, "Fairy")
show_text("A gentle fairy appears by a sparkling stream.", "Fairy", 60, 330)
set_choices({
    { text = "Talk to the fairy", scene = "scene_fairy.lua" },
    { text = "Return to the forest", scene = "scene1.lua" }
})
