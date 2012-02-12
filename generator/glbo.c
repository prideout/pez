
#include <stdio.h>

GlboPixels glboLoadPixels(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    unsigned int compressedSize, decompressedSize;
    unsigned char* compressed;
    unsigned int headerSize;
    GlboPixels pixels;

    fseek(file, 0, SEEK_END);
    compressedSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    compressed = (unsigned char*) malloc(compressedSize);
    fread(compressed, 1, compressedSize, file);
    fclose(file);

    decompressedSize = 0;
    lzfx_decompress(compressed, compressedSize, 0, &decompressedSize);

    pixels.RawHeader = (void*) malloc(decompressedSize);
    lzfx_decompress(compressed, compressedSize, pixels.RawHeader, &decompressedSize);
    free(compressed);

    headerSize = sizeof(struct GlboPixelsRec);
    memcpy(&pixels, pixels.RawHeader, headerSize - 2 * sizeof(void*));
    pixels.Frames = (char*) pixels.RawHeader + headerSize;

    return pixels;
}

void glboFreePixels(GlboPixels pixels)
{
    free(pixels.RawHeader);
}

void glboFreeVerts(GlboVerts verts)
{
    free(verts.RawHeader);
}

void glboSavePixels(GlboPixels pixels, const char* filename)
{
    unsigned int headerSize = sizeof(struct GlboPixelsRec);
    unsigned int contentSize = pixels.FrameCount * pixels.BytesPerFrame;
    unsigned int decompressedSize = headerSize + contentSize;
    unsigned char* decompressed;
    unsigned int compressedSize;
    unsigned char* compressed;
    FILE* file;
    
    decompressed = (unsigned char*) malloc(decompressedSize);
    memcpy(decompressed, &pixels, headerSize);
    memcpy(decompressed + headerSize, pixels.Frames, contentSize);

    compressedSize = decompressedSize;
    compressed = (unsigned char*) malloc(decompressedSize);
    lzfx_compress(decompressed, decompressedSize, compressed, &compressedSize);
    
    free(decompressed);

    file = fopen(filename, "wb");
    fwrite(compressed, 1, compressedSize, file);
    free(compressed);
    fclose(file);
}

GlboVerts glboLoadVerts(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    
    unsigned int compressedSize;
    fseek(file, 0, SEEK_END);
    compressedSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* compressed = (unsigned char*) malloc(compressedSize);
    fread(compressed, 1, compressedSize, file);
    fclose(file);

    unsigned int decompressedSize = 0;
    lzfx_decompress(compressed, compressedSize, 0, &decompressedSize);

    GlboVerts verts;
    verts.RawHeader = (void*) malloc(decompressedSize);
    lzfx_decompress(compressed, compressedSize, verts.RawHeader, &decompressedSize);
    free(compressed);

    unsigned int headerSize = sizeof(struct GlboVertsRec);
    memcpy(&verts, verts.RawHeader, headerSize - sizeof(void*));

    unsigned int attribTableSize = sizeof(struct GlboAttribRec) * verts.AttribCount;
    unsigned int indexTableSize = verts.IndexBufferSize;
    
    verts.Attribs = (GlboAttrib*) ((char*) verts.RawHeader + headerSize);
    verts.Indices = (GLvoid*) ((char*) verts.RawHeader + headerSize + attribTableSize);
    
    char* f = (char*) verts.RawHeader + headerSize + attribTableSize + indexTableSize;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        verts.Attribs[attrib].Frames = (GLvoid*) f;
        f += verts.VertexCount * verts.Attribs[attrib].FrameCount * verts.Attribs[attrib].Stride;
    }

    const char* s = f;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        verts.Attribs[attrib].Name = s;
        s += strlen(s) + 1;
    }

    return verts;
}

void glboSaveVerts(GlboVerts verts, const char* filename)
{
    unsigned int headerSize = sizeof(struct GlboVertsRec);
    unsigned int attribTableSize = sizeof(struct GlboAttribRec) * verts.AttribCount;
    unsigned int indexTableSize = verts.IndexBufferSize;

    unsigned int stringTableSize = 0;
    unsigned int frameTableSize = 0;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        stringTableSize += strlen(verts.Attribs[attrib].Name) + 1;
        frameTableSize += verts.VertexCount * verts.Attribs[attrib].FrameCount * verts.Attribs[attrib].Stride;
    }

    unsigned int decompressedSize = headerSize + attribTableSize + indexTableSize + frameTableSize + stringTableSize;

    unsigned char* decompressed = (unsigned char*) malloc(decompressedSize);
    memcpy(decompressed, &verts, headerSize);
    memcpy(decompressed + headerSize, verts.Attribs, attribTableSize);
    memcpy(decompressed + headerSize + attribTableSize, verts.Indices, indexTableSize);
    
    unsigned char* frameTable = decompressed + headerSize + attribTableSize + indexTableSize;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        GLsizei frameSize = verts.VertexCount * verts.Attribs[attrib].FrameCount * verts.Attribs[attrib].Stride;
        memcpy(frameTable, verts.Attribs[attrib].Frames, frameSize);
        frameTable += frameSize;
    }

    char* stringTable = (char*) frameTable;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        const char* s = verts.Attribs[attrib].Name;
        strcpy(stringTable, s);
        stringTable += strlen(s) + 1;
    }

    unsigned int compressedSize = decompressedSize;
    unsigned char* compressed = (unsigned char*) malloc(decompressedSize);
    lzfx_compress(decompressed, decompressedSize, compressed, &compressedSize);
    
    free(decompressed);

    FILE* file = fopen(filename, "wb");
    fwrite(compressed, 1, compressedSize, file);
    free(compressed);
    fclose(file);
}
