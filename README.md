![Image test](http://github.com/prideout/pez/raw/master/logo.png)

Pez lets you create a simple, fixed-size window and render to it with OpenGL.  It's composed of a single header file and single C99 source file.

This intended just for creating small demos on Linux.  If you're building a real application, you should probably use a real library like SDL or GLFW.

Your OpenGL demo needs to implement a few functions:

PezConfig PezInitialize()     | Do any one-time setup you want, return a small struct with window size & title
---------------------------   | -------------
void PezRender()              | Do your drawing here!
void PezUpdate(float seconds) | Perform animation and physics; you're given an elapsed time since the previous invocation

Additionally, some optional:

void PezHandleMouse(int x, int y, int action) | Respond to a mouse click or move event
void PezReceiveDrop(const char* filename)     | Respond to a file being drag-n-dropped to your window

License
-------

[Creative Commons Attribution](http://creativecommons.org/licenses/by/3.0/)
