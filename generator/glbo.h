
typedef struct GlboAttribRec {
    const GLchar* Name;
    GLint Size;
    GLenum Type;
    GLsizei Stride;
    int FrameCount;
    GLvoid* Frames;
} GlboAttrib;

typedef struct GlboVertsRec {
    int AttribCount;
    int IndexCount;
    int VertexCount;
    GLenum IndexType;
    GLsizeiptr IndexBufferSize;
    GlboAttrib* Attribs;
    GLvoid* Indices;
    void* RawHeader;
} GlboVerts;

typedef struct GlboPixelsRec {
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
} GlboPixels;

GlboVerts glboLoadVerts(const char* filename);
GlboVerts glboGenQuad(float left, float top, float right, float bottom);
void glboFreeVerts(GlboVerts verts);
void glboSaveVerts(GlboVerts verts, const char* filename);

GlboPixels glboLoadPixels(const char* filename);
void glboFreePixels(GlboPixels pixels);
void glboSavePixels(GlboPixels pixels, const char* filename);
void glboRenderText(GlboPixels pixels, const char* message);
GlboPixels glboGenNoise(GlboPixels desc, float alpha, float beta, int n);
