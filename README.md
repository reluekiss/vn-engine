For building on different platforms run
```console
$ make PLATFORM=windows
```

The ones that are supported by Raylib are Windows, Darwin (OSX) and Linux, so those are the ones we support too.

NOTE: Compiling for a platform requires you to be on the particular platform, this is just a simple Makefile and doesn't handle cross compilation without the specific tools.

NOTE: I have not done extensive testing on non Linux platforms as of 20/02/2025, so there may or may not be bugs present.

For using the engine a complete list of commands is below:
```
::bg
cat.png

::music
:start 00:15
country.mp3

::sprite
:id pirate 1
:pos 100x100
pirate.jpg

::text
:name pirate
:pos
yar

::sound
a.mp3

::Usprite
:id pirate 1

::scene
option 1
Test Module/scene2.txt
option 2
Test Module/scene3.txt
```
The ::end command tells the engine that you are ready to go back to the title screen.

It is recommended to create a directory for your game/module and only expose the Starting Scene file in the directory, see below for example folder structure:
```
├── Test Module
│   ├── scene2.txt
│   └── scene3.txt
└── scene1.txt
```
