![Image test](http://github.com/prideout/pez/raw/master/logo.png)

Pez lets you create a simple, fixed-size window and render to it with OpenGL.  It's composed of a single header file and single C99 source file.

It's intended for creating small demos on Linux.  If you're building a real application, you should probably use a real library like [SDL](http://libsdl.org/) or [GLFW](http://www.glfw.org/).

Your OpenGL demo needs to implement a few functions:

`PezConfig PezInitialize()`     | One-time setup, return a small struct with window size & title
---------------------------   | -------------
`void PezRender()`              | Do your drawing here!
`void PezUpdate(float elapsedSeconds)` | Perform animation and physics

You can also optionally implement the following handler functions, depending on which `#define` statements you've got enabled in the header file:

`void PezHandleMouse(int x, int y, int action)` | Respond to a mouse click or move event
--------------------------------------------- | --------------------------------------
`void PezReceiveDrop(const char* filename)`     | Respond to a file being drag-n-dropped to your window

Pez provides some utility functions to help make assertions with printf-style error messages:

void pezFatal(const char* pStr, ...) |
------------------------------------ |
void pezCheck(int condition, ...)    |
void pezPrintString(const char* pStr, ...) |

Note that functions that *you* implement are prefixed with a capital `Pez`, whereas functions that *pez* implements are prefixed with a lowercase `pez`.

Pez simplements some OS-specific stuff for you:

    int pezIsPressing(char key)
    const char* pezOpenFileDialog()
    const char* pezGetDesktopFolder()

Pez never makes any OpenGL calls (that's up to you), but it provides a few helper functions to make it easy to load GLSL strings:

    const char* pezGetShader(const char* effectKey)
    int pezAddDirective(const char* token, const char* directive)

And, some functions to make it easy to create vertex buffer objects:

    PezVerts pezLoadVerts(const char* filename)
    PezVerts pezGenQuad(float left, float top, float right, float bottom)
    void pezFreeVerts(PezVerts verts)
    void pezSaveVerts(PezVerts verts, const char* filename)

And, some functions to make it easy to create textures or pixel buffer objects:

    PezPixels pezLoadPixels(const char* filename)
    void pezFreePixels(PezPixels pixels)
    void pezSavePixels(PezPixels pixels, const char* filename)
    void pezRenderText(PezPixels pixels, const char* message)
    PezPixels pezGenNoise(PezPixels desc, float alpha, float beta, int n)

License
-------

[Creative Commons Attribution](http://creativecommons.org/licenses/by/3.0/)
