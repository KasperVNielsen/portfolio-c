if you have minGW 64 bit 
Go here: https://www.glfw.org/download.html

Under Windows pre-compiled binaries, grab:
64-bit Windows binaries (for MinGW if you use gcc, or MSVC if you use Visual Studio).

Extract the zip. Inside you’ll see:

include/GLFW/ → headers (glfw3.h, etc.)

you can follow this video if needed
https://www.youtube.com/watch?v=Y4F0tI7WlDs

compile: gcc src/main.c src/helpers.c src/glad.c -Iinclude -Llib -lglfw3dll -lopengl32 -lgdi32 -o pf.exe
run: ./pf.exe