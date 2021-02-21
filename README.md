# Tilec
An opensource tile map editor in C, using SDL2 and OpenGL, also running on Android.
Based on [some](https://github.com/renehorstmann/some) framework.
Forked from [Pixelc](https://github.com/renehorstmann/Pixelc)


## Todo
- palette shows tile sheets
- palette change buttons or gesture
- canvas draws tiles instead of the map
- animation button removes layer alpha and just animates the canvas

## Compiling on Windows
Compiling with Mingw (msys2).
Currently not working with cmake, but with the following gcc call.
I had to put all source files into one dir (from src/e/*, r/*, p/*, u/* into src/*) to get the linker happy.
```
gcc -o tilec src/* -Iinclude $(sdl2-config --cflags --libs) -lSDL2_image -lSDL2_ttf -lglew32 -lopengl32 -lglu32 -DUSING_GLEW
```
