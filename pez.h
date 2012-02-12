#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PEZ_MAINLOOP 1

#define PEZ_MOUSE_HANDLER 1

#define PEZ_DROP_HANDLER 1

#include "glew.h"

#define PEZ_FORWARD_COMPATIBLE_GL 1

typedef struct PezConfigRec
{
    const char* Title;
    int Width;
    int Height;
    int Multisampling;
    int VerticalSync;
} PezConfig;

#ifdef PEZ_MAINLOOP
PezConfig PezGetConfig();
void PezInitialize();
void PezRender();
void PezUpdate(float seconds);

#ifdef PEZ_MOUSE_HANDLER
void PezHandleMouse(int x, int y, int action);
#endif

#ifdef PEZ_DROP_HANDLER
void PezReceiveDrop(const char* filename);
#endif

#else
void pezSwapBuffers();
#endif

enum {PEZ_DOWN, PEZ_UP, PEZ_MOVE, PEZ_DOUBLECLICK};
#define TwoPi (6.28318531f)
#define Pi (3.14159265f)
#define countof(A) (sizeof(A) / sizeof(A[0]))

void pezPrintString(const char* pStr, ...);
void pezFatal(const char* pStr, ...);
void pezCheck(int condition, ...);
int pezIsPressing(char key);
const char* pezResourcePath();
const char* pezOpenFileDialog();
const char* pezGetDesktopFolder();

int pezInit();
int pezShutdown();
int pezAddPath(const char* pathPrefix, const char* pathSuffix);
const char* pezGetShader(const char* effectKey);
const char* pezGetError();
int pezAddDirective(const char* token, const char* directive);


typedef struct PezAttribRec {
    const GLchar* Name;
    GLint Size;
    GLenum Type;
    GLsizei Stride;
    int FrameCount;
    GLvoid* Frames;
} PezAttrib;

typedef struct PezVertsRec {
    int AttribCount;
    int IndexCount;
    int VertexCount;
    GLenum IndexType;
    GLsizeiptr IndexBufferSize;
    PezAttrib* Attribs;
    GLvoid* Indices;
    void* RawHeader;
} PezVerts;

typedef struct PezPixelsRec {
    int FrameCount;
    GLsizei Width;
    GLsizei Height;
    GLsizei Depth;
    GLint MipLevels;
    GLenum Format;
    GLenum InternalFormat;
    GLenum Type;
    GLsizeiptr BytesPerFrame;
    GLvoid* Frames;
    void* RawHeader;
} PezPixels;

PezVerts pezLoadVerts(const char* filename);
PezVerts pezGenQuad(float left, float top, float right, float bottom);
void pezFreeVerts(PezVerts verts);
void pezSaveVerts(PezVerts verts, const char* filename);

PezPixels pezLoadPixels(const char* filename);
void pezFreePixels(PezPixels pixels);
void pezSavePixels(PezPixels pixels, const char* filename);
void pezRenderText(PezPixels pixels, const char* message);
PezPixels pezGenNoise(PezPixels desc, float alpha, float beta, int n);
#ifdef __cplusplus
}
#endif
