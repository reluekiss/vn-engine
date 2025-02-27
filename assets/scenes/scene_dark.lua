load_background("bg_dark_forest.jpg")
show_text("The shadows shift as you move deeper. Danger lurks in every corner...", "Narrator", {r = 0, g = 0, b = 0, a = 255 }, 60, 330, {r = 255, g = 0, b = 0, a = 255})
set_choices({
    { text = "Proceed cautiously", scene = "scene_caution.lua" },
    { text = "Run back", scene = "clearing.lua" }
})
