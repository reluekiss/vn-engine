fairy = { name = "Fairy", color = { r = 255, g = 0, b = 0, a = 0 }, x = 200, y = 150, file = "fairy.png"}
load_sprite(fairy.file, fairy.x, fairy.y, fairy.name )
show_text("A gentle fairy appears by a sparkling stream.", fairy.name, fairy.color, 60, 330)
set_choices({
    { text = "Talk to the fairy", scene = "scene_fairy.lua" },
    { text = "Return to the forest", scene = "clearing.lua" }
})
