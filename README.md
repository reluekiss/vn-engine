To make, run:
```console
$ make && make run
```

The current API exposes the following C functions:
```
void load_background(string filepath) // Draw a background until a new background is loaded.
void load_sprite(string filepath, float x, float y, string id) // Draw a sprite to a screen until it is unloaded.
void unload_sprite(string id) // Unload a sprite so that it is no longer drawn.
void play_music(string filepath, float startTime) // Play a song until a new one is loaded (loops)
void play_sound(string filepath) // Play a sound once.
void show_text(string text, string name, float x, float y, table textColor) // Draws text.
void clear_text() // Clears the current text.
void set_choices(table choice) // Creates a list of buttons which move you to a new scene.
void quit() // Exit program.
void module_init(string folder) // Sets a prefix folder to access scenes from.

// under construction
int idx create_character(table color)
void destroy_character(int idx)
void modify_character(int idx, table color)
```

And the following global variables:
```
last_scene
```

The tables given as inputs to the C functions have the following defined types in C, however keep in mind that you can have as much additional data on these tables as long as it is after the defined fields (assets/scenes/water_scene.lua):
```
color = { uint8 r, uint8 g, uint8 b, uint8 a }
choice = { string text, string scene )
```

To see an example look inside mods. It is recommeded to create a new folder in which to store the additional scene files to not clutter the scenes folder and to allow for easier differentiation between projects, use module_init for applying a prefix.

DISCLAIMER: I make no claims of ownership over any of the binary assets of included libraries under the externals directory, furthermore their functioning is not at the discretions of their creators and may behave differently then expected due to changes I have made to them.
