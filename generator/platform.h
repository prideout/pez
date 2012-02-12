
#include "glew.h"

#define ZEP_FORWARD_COMPATIBLE_GL 1

typedef struct ZepConfigRec
{
    const char* Title;
    int Width;
    int Height;
    int Multisampling;
    int VerticalSync;
} ZepConfig;

#ifdef ZEP_MAINLOOP
ZepConfig ZepGetConfig();
void ZepInitialize();
void ZepRender();
void ZepUpdate(float seconds);

#ifdef ZEP_MOUSE_HANDLER
void ZepHandleMouse(int x, int y, int action);
#endif

#ifdef ZEP_DROP_HANDLER
void ZepReceiveDrop(const char* filename);
#endif

#else
void zepSwapBuffers();
#endif

enum {ZEP_DOWN, ZEP_UP, ZEP_MOVE, ZEP_DOUBLECLICK};
#define TwoPi (6.28318531f)
#define Pi (3.14159265f)
#define countof(A) (sizeof(A) / sizeof(A[0]))

void zepPrintString(const char* pStr, ...);
void zepFatal(const char* pStr, ...);
void zepCheck(int condition, ...);
int zepIsPressing(char key);
const char* zepResourcePath();
const char* zepOpenFileDialog();
const char* zepGetDesktopFolder();
