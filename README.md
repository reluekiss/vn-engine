To make, run:
```console
$ make && make run
```

The current API exposes the following C functions:
```
load_background(string filepath)
load_sprite(string filepath, float x, float y, string id)
unload_sprite(string id)
play_music(string filepath, float startTime)
play_sound(string filepath)
show_text(string text, string name, float x, float y, table textColor)
clear_text()
set_choices(table choice)
quit()
```

And the following global variables:
```
last_scene
```

The tables given as inputs to the C functions have the following defined types in C, however keep in mind that you can have as much additional data on these tables as long as it is after the defined fields (assets/scenes/water_scene.lua):
```
color = { int r, int g, int b, int a }
choice = { string choiceText, string filepath )
```

To see an example look inside assets/scenes, currently the initial scene is hard coded to be main.lua.

DISCLAIMER: I make no claims of ownership over any of the binary assets of included libraries under the externals directory, furthermore their functioning is not at the discretions of their creators and may behave differently then expected due to changes I have made to them.
