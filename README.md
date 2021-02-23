# Tilec
An opensource tile map editor in C, using SDL2 and OpenGL, also running on Android.
Based on [some](https://github.com/renehorstmann/some) framework.
Forked from [Pixelc](https://github.com/renehorstmann/Pixelc)

## Status:
Ready to draw maps.
Saves after each tip on the screen to tilemap.png.
Loads tilemap.png at start, if available.
Put tile sheets into tiles/* to load them.
The tile sheets names must be tile_xx.png, starting with tile_01.png.
An import button can load import.png as selection, if available.
Palette, canvas size, animation size can be configured in code (main.c).

## Todo
- animation button removes layer alpha and just animates the canvas

## Compiling on Windows
Compiling with Mingw (msys2).
Currently not working with cmake, but with the following gcc call.
I had to put all source files into one dir (from src/e/*, r/*, p/*, u/* into src/*) to get the linker happy.
```
gcc -o tilec src/* -Iinclude $(sdl2-config --cflags --libs) -lSDL2_image -lSDL2_ttf -lglew32 -lopengl32 -lglu32 -DUSING_GLEW
```
