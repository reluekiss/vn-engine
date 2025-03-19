fairy = { name = "Fairy", color = { r = 150, g = 0, b = 50, a = 255 }, x = 200, y = 150, file = "fairy.png"}
load_sprite(fairy.file, fairy.x, fairy.y, fairy.name )
show_text(fairy, "A gentle fairy appears by a sparkling stream.")
set_choices({
    { text = "Talk to the fairy", scene = "scene_fairy.lua" },
    { text = "Return to the forest", scene = "clearing.lua" }
})
