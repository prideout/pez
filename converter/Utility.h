#pragma once
#include "pez.h"

struct TexturePod {
    GLuint Handle;
    GLsizei Width;
    GLsizei Height;
};

TexturePod OverlayText(const char* message);
void ExportScreenshot(const char* filename);
