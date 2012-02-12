#include "pez.h"
#include "Utility.h"
#include "vmath.h"
#include "hdrloader.h"
#include <stdio.h>
#include <ImfRgbaFile.h>
#include <ImfArray.h>
#include <ImathBox.h>
#include <openctm.h>

static enum { Empty, HasTexture, HasMesh } Mode = Empty;

GLuint QuadProgram;
GLuint QuadVao;
GLuint ScreenVao;
GLuint MeshProgram;
GLuint MeshVao;
GLsizei MeshIndexCount;
GLsizei MeshVertexCount;
GLuint CurrentTexture;
Matrix4 ModelviewMatrix;
Matrix4 ProjectionMatrix;
Matrix3 NormalMatrix;
GLfloat PackedNormalMatrix[9];
bool ShowWireframe = false;
bool ConvertToHalfFloat = true;

GLuint LoadProgram(const char* vs, const char* gs, const char* fs);
GLuint CurrentProgram();
GLuint CreateQuad(int sourceWidth, int sourceHeight, int destWidth, int destHeight);

#define u(x) glGetUniformLocation(CurrentProgram(), x)
#define a(x) glGetAttribLocation(CurrentProgram(), x)
#define offset(x) ((const GLvoid*)x)

PezConfig PezGetConfig()
{
    PezConfig config;
    config.Title = "pez-viewer";
    config.Width = 853;
    config.Height = 480;
    config.Multisampling = 1;
    config.VerticalSync = 0;
    return config;
}

void SavePbo(const char* filename)
{
    PezPixels pixels = {0};
    GLenum redType;
    int redSize, alpSize;
    GLint lod = 0;
    
    GLboolean performResize = GL_TRUE;
    if (performResize) {
    
        // Configure the new dimensions:
        const int newWidth = 2000;
        const int newHeight = 1200;
        
        // Create the FBO:
        GLuint fboHandle, fboTexture, colorBuffer;
        glGenFramebuffers(1, &fboHandle);
        glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);
        glGenTextures(1, &fboTexture);
        glBindTexture(GL_TEXTURE_2D, fboTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, newWidth, newHeight, 0, GL_RGB, GL_HALF_FLOAT, 0);
        glGenRenderbuffers(1, &colorBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);
        pezCheck(GL_NO_ERROR == glGetError(), "Unable to attach color buffer");
        pezCheck(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER), "Unable to create FBO.");
        
        // Obtain some sizing information:
        PezConfig cfg = PezGetConfig();
        glBindTexture(GL_TEXTURE_2D, CurrentTexture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_WIDTH, &pixels.Width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_HEIGHT, &pixels.Height);

        // Blit the texture onto the FBO:
        glViewport(0, 0, newWidth, newHeight);
        QuadVao = CreateQuad(newWidth, -newHeight, newWidth, newHeight);
        glBindTexture(GL_TEXTURE_2D, CurrentTexture);
        glUseProgram(QuadProgram);
        glBindVertexArray(QuadVao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
        // Swap the FBO texture with the loaded texture:
        CurrentTexture = fboTexture;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, cfg.Width, cfg.Height);
        QuadVao = CreateQuad(newWidth, newHeight, cfg.Width, cfg.Height);
        glViewport(0, 0, cfg.Width, cfg.Height);
    }
    
    glBindTexture(GL_TEXTURE_2D, CurrentTexture);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_WIDTH, &pixels.Width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_HEIGHT, &pixels.Height);

    GLboolean findMinMax = GL_TRUE;
    if (findMinMax) {
        void* temp = malloc(12 * pixels.Width * pixels.Height);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, temp);
        Vector3 minRgb = V3MakeFromScalar(1000.0f);
        Vector3 maxRgb = V3MakeFromScalar(-1000.0f);
        float* rgb = (float*) temp;
        for (int i = 0; i < pixels.Width * pixels.Height; i++) {
            float r = *rgb++;
            float g = *rgb++;
            float b = *rgb++;
            minRgb.x = minRgb.x < r ? minRgb.x : r;
            minRgb.y = minRgb.y < g ? minRgb.y : g;
            minRgb.z = minRgb.z < b ? minRgb.z : b;
            maxRgb.x = maxRgb.x > r ? maxRgb.x : r;
            maxRgb.y = maxRgb.y > g ? maxRgb.y : g;
            maxRgb.z = maxRgb.z > b ? maxRgb.z : b;
        }
        free(temp);
        printf("Min = (%f,%f,%f)\n", minRgb.x, minRgb.y, minRgb.z);
        printf("Max = (%f,%f,%f)\n", maxRgb.x, maxRgb.y, maxRgb.z);
    }

    glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_DEPTH, &pixels.Depth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_INTERNAL_FORMAT, (GLint*) &pixels.InternalFormat);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_RED_TYPE, (GLint*) &redType);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_RED_SIZE, &redSize);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, lod, GL_TEXTURE_ALPHA_SIZE, &alpSize);
    
    if (redType == GL_FLOAT && (redSize == 16 || redSize == 32)) {
        pixels.Format = alpSize ? GL_RGBA : GL_RGB;
        
        // Ask the driver to convert 32-bit floats to 16-bit floats:
        if (redSize == 32 && ConvertToHalfFloat) {
            redSize = 16;
            pixels.InternalFormat = alpSize ? GL_RGBA16F : GL_RGB16F;
        }

        int bytesPerPixel;
        if (redSize == 16) {
            pixels.Type = GL_HALF_FLOAT;
            bytesPerPixel = alpSize ? 8 : 6;
        } else {
            pixels.Type = GL_FLOAT;
            bytesPerPixel = alpSize ? 16 : 12;
        }
        
        pixels.BytesPerFrame = bytesPerPixel * pixels.Width * pixels.Height;

    } else {
        pezFatal("Texture %d (%d x %d) has unsupported format 0x%04.4x (%d bits per component)",
            CurrentTexture, pixels.Width, pixels.Height, redType, redSize);
    }
    
    pixels.FrameCount = 1;
    pixels.MipLevels = 1;
    pixels.Frames = malloc(pixels.BytesPerFrame * pixels.FrameCount);
    pixels.RawHeader = 0;

    glGetTexImage(GL_TEXTURE_2D, lod, pixels.Format, pixels.Type, pixels.Frames);

    pezSavePixels(pixels, filename);
    
    free(pixels.Frames);
}

void SaveVbo(const char* filename)
{
    glBindVertexArray(MeshVao);
    glUseProgram(MeshProgram);

    int frameCount = 1;
    PezAttrib attribs[] = {
        {"Position", 3, GL_FLOAT, sizeof(float) * 3, frameCount, 0 },
        {"Normal", 3, GL_FLOAT, sizeof(float) * 3, frameCount, 0 },
    };

    PezVerts verts;
    verts.AttribCount = sizeof(attribs) / sizeof(attribs[0]);
    
    for (int i = 0; i < verts.AttribCount; i++) {
        GLuint vbo;
        GLuint slot = a(attribs[i].Name);
        glGetVertexAttribiv(slot, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, (GLint*) &vbo);
        pezCheck(glGetError() == GL_NO_ERROR, "Unable to query attribute '%s' (%d)", attribs[i].Name, slot);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        GLvoid* sourceData = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        pezCheck(sourceData != 0, "glMapBuffer returned NULL for '%s' (%d)", attribs[i].Name, slot);
        
        GLint bufferSize;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
        attribs[i].Frames = malloc(bufferSize);
        memcpy(attribs[i].Frames, sourceData, bufferSize);
        
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    verts.Attribs = &attribs[0];
    verts.IndexCount = MeshIndexCount;
    verts.VertexCount = MeshVertexCount;
    verts.IndexType = GL_UNSIGNED_INT;
    verts.Indices  = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_ONLY);
    verts.RawHeader = 0;
    
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &verts.IndexBufferSize);
    
    pezSaveVerts(verts, filename);

    for (int i = 0; i < verts.AttribCount; i++)
        free(attribs[i].Frames);

    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
}

GLuint CreateQuad(int sourceWidth, int sourceHeight, int destWidth, int destHeight)
{
    // Stretch to fit:
    float q[] = {
        -1, -1, 0, 1,
        +1, -1, 1, 1,
        -1, +1, 0, 0,
        +1, +1, 1, 0 };
        
    if (sourceHeight < 0) {
        sourceHeight = -sourceHeight;
        q[3] = 1-q[3];
        q[7] = 1-q[7];
        q[11] = 1-q[11];
        q[15] = 1-q[15];
    }

    float sourceRatio = (float) sourceWidth / sourceHeight;
    float destRatio = (float) destWidth  / destHeight;
    
    // Horizontal fit:
    if (sourceRatio > destRatio) {
        q[1] = q[5] = -destRatio / sourceRatio;
        q[9] = q[13] = destRatio / sourceRatio;

    // Vertical fit:    
    } else {
        q[0] = q[8] = -sourceRatio / destRatio;
        q[4] = q[12] = sourceRatio / destRatio;
    }

    GLuint vbo, vao;
    
    glUseProgram(QuadProgram);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
    glVertexAttribPointer(a("Position"), 2, GL_FLOAT, GL_FALSE, 16, 0);
    glVertexAttribPointer(a("TexCoord"), 2, GL_FLOAT, GL_FALSE, 16, offset(8));
    glEnableVertexAttribArray(a("Position"));
    glEnableVertexAttribArray(a("TexCoord"));
    
    return vao;
}

void PezInitialize()
{
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    QuadProgram = LoadProgram("shaders.Quad.VS", 0, "shaders.Quad.FS");
    MeshProgram = LoadProgram("shaders.Mesh.VS", 0, "shaders.Mesh.FS");
    glGenTextures(1, &CurrentTexture);
    
    PezConfig cfg = PezGetConfig();
    ScreenVao = CreateQuad(cfg.Width, cfg.Height, cfg.Width, cfg.Height);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void PezRender()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (Mode == HasTexture) {
    
        glUseProgram(QuadProgram);
        glBindVertexArray(QuadVao);
        glBindTexture(GL_TEXTURE_2D, CurrentTexture);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    } else if (Mode == HasMesh) {

        if (ShowWireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        glEnable(GL_DEPTH_TEST);
        glUseProgram(MeshProgram);
        glBindVertexArray(MeshVao);
        glUniformMatrix4fv(u("Projection"), 1, 0, &ProjectionMatrix.col0.x);
        glUniformMatrix4fv(u("Modelview"), 1, 0, &ModelviewMatrix.col0.x);
        glUniformMatrix3fv(u("NormalMatrix"), 1, 0, &PackedNormalMatrix[0]);
        glDrawElements(GL_TRIANGLES, MeshIndexCount, GL_UNSIGNED_INT, 0);
        glDisable(GL_DEPTH_TEST);
        
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (Mode != Empty) {

        glUseProgram(QuadProgram);
        TexturePod messageTexture = OverlayText("Spacebar to save.");
        glEnable(GL_BLEND);
        glBindVertexArray(ScreenVao);
        glBindTexture(GL_TEXTURE_2D, messageTexture.Handle);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_BLEND);
    }
}

void PezUpdate(float seconds)
{
    static float theta = 0;
    theta += seconds * 0.00005;

    Matrix4 rotation = M4MakeRotationY(theta * Pi / 180.0f);
    Matrix4 translation = M4MakeTranslation(V3MakeFromElems(0, 0, -20));
    ModelviewMatrix = M4Mul(translation, rotation);
    NormalMatrix = M4GetUpper3x3(ModelviewMatrix);
    
    for (int i = 0; i < 9; ++i)
        PackedNormalMatrix[i] = M3GetElem(NormalMatrix, i/3, i%3);

    PezConfig cfg = PezGetConfig();
    const float x = 0.6f;
    const float y = x * cfg.Height / cfg.Width;
    const float left = -x, right = x;
    const float bottom = -y, top = y;
    const float zNear = 4, zFar = 100;
    ProjectionMatrix = M4MakeFrustum(left, right, bottom, top, zNear, zFar);
    
    if (pezIsPressing('W'))
        ShowWireframe = !ShowWireframe;
    
    if (pezIsPressing(' ') && Mode != Empty) {
        const char* filename = pezOpenFileDialog();
        if (Mode == HasMesh)
            SaveVbo(filename);
        else if (Mode == HasTexture)
            SavePbo(filename);
    }
}

void PezHandleMouse(int x, int y, int action)
{
}

void LoadHdr(const char* filename)
{
    pezPrintString("Loading HDR image: %s\n", filename);

    HDRLoaderResult result;
    HDRLoader::load(filename, result);
    
    pezPrintString("Image size is %d x %d\n", result.width, result.height);

    glBindTexture(GL_TEXTURE_2D, CurrentTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, result.width, result.height, 0,
                 GL_RGB, GL_FLOAT, result.cols);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    PezConfig cfg = PezGetConfig();
    QuadVao = CreateQuad(result.width, result.height, cfg.Width, cfg.Height);
    Mode = HasTexture;
}

void LoadPbo(const char* filename)
{
    pezPrintString("Loading PBO image: %s\n", filename);

    PezPixels pixels = pezLoadPixels(filename);
    
    pezPrintString("Image size is %d x %d\n", pixels.Width, pixels.Height);

    glBindTexture(GL_TEXTURE_2D, CurrentTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, pixels.InternalFormat, pixels.Width, pixels.Height, 0,
                 pixels.Format, pixels.Type, pixels.Frames);
    pezFreePixels(pixels);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    PezConfig cfg = PezGetConfig();
    QuadVao = CreateQuad(pixels.Width, pixels.Height, cfg.Width, cfg.Height);
    Mode = HasTexture;
}

void LoadExr(const char* filename)
{
    using namespace Imf;
    using namespace Imath;

    pezPrintString("Loading EXR image: %s\n", filename);
    
    RgbaInputFile file(filename);
    Box2i dw = file.dataWindow();
    int width = dw.max.x - dw.min.x + 1;
    int height = dw.max.y - dw.min.y + 1;
    
    Array2D<Rgba> pixels;
    pixels.resizeErase (height, width);
    Rgba* rawPixels = &pixels[0][0];
    
    file.setFrameBuffer (rawPixels - dw.min.x - dw.min.y * width, 1, width);
    file.readPixels (dw.min.y, dw.max.y);

    glBindTexture(GL_TEXTURE_2D, CurrentTexture);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
                 GL_RGBA, GL_HALF_FLOAT, rawPixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    PezConfig cfg = PezGetConfig();
    QuadVao = CreateQuad(width, height, cfg.Width, cfg.Height);
    Mode = HasTexture;
}

void LoadVbo(const char* filename)
{
    pezPrintString("Loading mesh: %s\n", filename);

    PezVerts verts = pezLoadVerts(filename);
    
    MeshVertexCount = verts.VertexCount;
    MeshIndexCount = verts.IndexCount;

    glUseProgram(MeshProgram);
    glGenVertexArrays(1, &MeshVao);
    glBindVertexArray(MeshVao);

    // Create a VBO for each attribute:
    PezAttrib* attrib = verts.Attribs;
    for (int i = 0; i < verts.AttribCount; i++, attrib++) {
        GLsizeiptr size = MeshVertexCount * attrib->Stride;
        GLuint slot = a(attrib->Name);
        GLuint handle;
        glGenBuffers(1, &handle);
        glBindBuffer(GL_ARRAY_BUFFER, handle);
        glBufferData(GL_ARRAY_BUFFER, size, attrib->Frames, GL_STATIC_DRAW);
        glEnableVertexAttribArray(slot);
        glVertexAttribPointer(slot,
            verts.Attribs[0].Size,
            verts.Attribs[0].Type, GL_FALSE,
            verts.Attribs[0].Stride, 0);
    }

    // Create the VBO for indices:
    GLsizeiptr size = MeshIndexCount * sizeof(CTMuint);
    GLuint handle;
    glGenBuffers(1, &handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, verts.Indices, GL_STATIC_DRAW);
    
    pezFreeVerts(verts);

    Mode = HasMesh;
}

void LoadCtm(const char* filename)
{
    pezPrintString("Loading mesh: %s\n", filename);

    // Open the CTM file:
    CTMcontext ctmContext = ctmNewContext(CTM_IMPORT);
    ctmLoad(ctmContext, filename);
    pezCheck(ctmGetError(ctmContext) == CTM_NONE, "OpenCTM Issue");
    MeshVertexCount = ctmGetInteger(ctmContext, CTM_VERTEX_COUNT);
    MeshIndexCount = 3 * ctmGetInteger(ctmContext, CTM_TRIANGLE_COUNT);

    glUseProgram(MeshProgram);
    glGenVertexArrays(1, &MeshVao);
    glBindVertexArray(MeshVao);

    // Create the VBO for positions:
    const CTMfloat* positions = ctmGetFloatArray(ctmContext, CTM_VERTICES);
    if (positions) {
        GLuint handle;
        GLsizeiptr size = MeshVertexCount * sizeof(float) * 3;
        glGenBuffers(1, &handle);
        glBindBuffer(GL_ARRAY_BUFFER, handle);
        glBufferData(GL_ARRAY_BUFFER, size, positions, GL_STATIC_DRAW);
        glEnableVertexAttribArray(a("Position"));
        glVertexAttribPointer(a("Position"), 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
    }

    // Create the VBO for normals:
    const CTMfloat* normals = ctmGetFloatArray(ctmContext, CTM_NORMALS);
    if (normals) {
        GLuint handle;
        GLsizeiptr size = MeshVertexCount * sizeof(float) * 3;
        glGenBuffers(1, &handle);
        glBindBuffer(GL_ARRAY_BUFFER, handle);
        glBufferData(GL_ARRAY_BUFFER, size, normals, GL_STATIC_DRAW);
        glEnableVertexAttribArray(a("Normal"));
        glVertexAttribPointer(a("Normal"), 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
    }

    // Create the VBO for indices:
    const CTMuint* indices = ctmGetIntegerArray(ctmContext, CTM_INDICES);
    if (indices) {
        GLuint handle;
        GLsizeiptr size = MeshIndexCount * sizeof(CTMuint);
        glGenBuffers(1, &handle);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
    }

    Mode = HasMesh;
}

void PezReceiveDrop(const char* filename)
{
    using namespace std;
    
    string stlFile( filename );
    size_t sep = stlFile.find_last_of( "." );
    if (sep == string::npos) {
        pezPrintString("Unrecognized file: %s\n", filename);
        return;
    }
    
    string extension = stlFile.substr(sep + 1);
    
    if (extension == "exr")
        LoadExr(filename);
    else if (extension == "ctm")
        LoadCtm(filename);
    else if (extension == "vbo")
        LoadVbo(filename);
    else if (extension == "pbo")
        LoadPbo(filename);
    else if (extension == "hdr")
        LoadHdr(filename);
    else
        pezPrintString("Unrecognized file: %s\n", filename);
}

GLuint LoadProgram(const char* vsKey, const char* gsKey, const char* fsKey)
{
    const char* vsSource = pezGetShader(vsKey);
    const char* gsSource = pezGetShader(gsKey);
    const char* fsSource = pezGetShader(fsKey);

    const char* msg = "Can't find %s shader: '%s'.\n";
    pezCheck(vsSource != 0, msg, "vertex", vsKey);
    pezCheck(gsKey == 0 || gsSource != 0, msg, "geometry", gsKey);
    pezCheck(fsKey == 0 || fsSource != 0, msg, "fragment", fsKey);
    
    GLint compileSuccess;
    GLchar compilerSpew[256];
    GLuint programHandle = glCreateProgram();

    GLuint vsHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsHandle, 1, &vsSource, 0);
    glCompileShader(vsHandle);
    glGetShaderiv(vsHandle, GL_COMPILE_STATUS, &compileSuccess);
    glGetShaderInfoLog(vsHandle, sizeof(compilerSpew), 0, compilerSpew);
    pezCheck(compileSuccess, "Can't compile %s:\n%s", vsKey, compilerSpew);
    glAttachShader(programHandle, vsHandle);

    GLuint gsHandle;
    if (gsKey) {
        gsHandle = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(gsHandle, 1, &gsSource, 0);
        glCompileShader(gsHandle);
        glGetShaderiv(gsHandle, GL_COMPILE_STATUS, &compileSuccess);
        glGetShaderInfoLog(gsHandle, sizeof(compilerSpew), 0, compilerSpew);
        pezCheck(compileSuccess, "Can't compile %s:\n%s", gsKey, compilerSpew);
        glAttachShader(programHandle, gsHandle);
    }
    
    GLuint fsHandle;
    if (fsKey) {
        fsHandle = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fsHandle, 1, &fsSource, 0);
        glCompileShader(fsHandle);
        glGetShaderiv(fsHandle, GL_COMPILE_STATUS, &compileSuccess);
        glGetShaderInfoLog(fsHandle, sizeof(compilerSpew), 0, compilerSpew);
        pezCheck(compileSuccess, "Can't compile %s:\n%s", fsKey, compilerSpew);
        glAttachShader(programHandle, fsHandle);
    }

    glLinkProgram(programHandle);
    
    GLint linkSuccess;
    glGetProgramiv(programHandle, GL_LINK_STATUS, &linkSuccess);
    glGetProgramInfoLog(programHandle, sizeof(compilerSpew), 0, compilerSpew);

    if (!linkSuccess) {
        pezPrintString("Link error.\n");
        if (vsKey) pezPrintString("Vertex Shader: %s\n", vsKey);
        if (gsKey) pezPrintString("Geometry Shader: %s\n", gsKey);
        if (fsKey) pezPrintString("Fragment Shader: %s\n", fsKey);
        pezPrintString("%s\n", compilerSpew);
    }
    
    return programHandle;
}

GLuint CurrentProgram()
{
    GLuint p;
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*) &p);
    return p;
}
