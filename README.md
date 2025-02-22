To make, run:
```console
$ make && make run
```

The current API exposes the following C functions:
```
load_background
load_sprite
unload_sprite
play_music
play_sound
show_texto
clear_text
set_choices
quit
```

And the following global variables:
```
last_scene
```

To see an example look inside assets/scenes, currently the initial scene is hard coded to be main.lua.
