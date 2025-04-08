To make, run:
```console
$ make && make run
```

The current API exposes the following C functions:
```
load_background(string filepath) // Draw a background until a new background is loaded.
load_sprite(string filepath, float x, float y, string id) // Draw a sprite to a screen until it is unloaded.
unload_sprite(string id) // Unload a sprite so that it is no longer drawn.
play_music(string filepath, float startTime) // Play a song until a new one is loaded (loops)
play_sound(string filepath) // Play a sound once.
show_text(table character, string text, table textColor, float x, float y) // Draws text.
clear_text() // Clears the current text.
set_choices(table choice) // Creates a list of buttons which move you to a new scene.
next_scene(string file) // Goes to next scene (ie allows you to have a stable save point)
quit() // Exit program.
module_init(string folder) // Sets a prefix folder to access scenes from.
pop_state() // pop off the gamestate stack to rollback to a previous state
```

And the following global variables:
```
last_scene
```

The tables given as inputs to the C functions have the following defined types in C, however keep in mind that you can have as much additional data on these tables as long as it is after the defined fields (assets/scenes/water_scene.lua):
```
color = { uint8 r, uint8 g, uint8 b, uint8 a }
choice = { string text, string scene )
character = { string name, table color }
```

To see an example look inside mods. It is recommeded to create a new folder in which to store the additional scene files to not clutter the scenes folder and to allow for easier differentiation between projects, use module_init for applying a prefix.

DISCLAIMER: I make no claims of ownership over any of the binary assets of included libraries under the externals directory, furthermore their functioning is not at the discretions of their creators and may behave differently then expected due to changes I have made to them.

TODO:
use PATH_SEPERATOR macro for paths e.g.

```C
strcpy(fileNameToLoad, TextFormat("%s" PATH_SEPERATOR "%s", fileDialogState.dirPathText, fileDialogState.fileNameText));
```
