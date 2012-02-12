// Zep was developed by Philip Rideout and released under the MIT License.

#include "zep.h"
#include "glew.h"
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <wchar.h>

typedef struct PlatformContextRec
{
    Display* MainDisplay;
    Window MainWindow;
} PlatformContext;

unsigned int GetMicroseconds()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000000 + tp.tv_usec;
}

int main(int argc, char** argv)
{
    int attrib[] = {
        GLX_SAMPLES, 4,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_SAMPLE_BUFFERS, 1,
        None
    };
    
    attrib[1] = ZepGetConfig().Multisampling ? 4 : 1;
 
    PlatformContext context;

    context.MainDisplay = XOpenDisplay(NULL);
    int screen = DefaultScreen(context.MainDisplay);
    Window root = RootWindow(context.MainDisplay, screen);

    int fbcount;
    PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)glXGetProcAddress((GLubyte*)"glXChooseFBConfig");
    GLXFBConfig *fbc = glXChooseFBConfig(context.MainDisplay, screen, attrib, &fbcount);
    if (!fbc)
        zepFatal("Failed to retrieve a framebuffer config\n");;

    PFNGLXGETVISUALFROMFBCONFIGPROC glXGetVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)glXGetProcAddress((GLubyte*)"glXGetVisualFromFBConfig");
    XVisualInfo *visinfo = glXGetVisualFromFBConfig(context.MainDisplay, fbc[0]);
    if (!visinfo)
        zepFatal("Error: couldn't create OpenGL window with this pixel format.\n");

    XSetWindowAttributes attr;
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(context.MainDisplay, root, visinfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask |
                      PointerMotionMask | ButtonPressMask | ButtonReleaseMask;

    context.MainWindow = XCreateWindow(
        context.MainDisplay,
        root,
        0, 0,
        ZepGetConfig().Width, ZepGetConfig().Height, 0,
        visinfo->depth,
        InputOutput,
        visinfo->visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &attr
    );
    XMapWindow(context.MainDisplay, context.MainWindow);

    GLXContext glcontext;
    if (0 && ZEP_FORWARD_COMPATIBLE_GL) {
        GLXContext tempContext = glXCreateContext(context.MainDisplay, visinfo, NULL, True);
        PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribs = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((GLubyte*)"glXCreateContextAttribsARB");
        if (!glXCreateContextAttribs) {
            zepFatal("Your platform does not support OpenGL 4.0.\n"
                          "Try changing ZEP_FORWARD_COMPATIBLE_GL to 0.\n");
        }
        int fbcount = 0;
        GLXFBConfig *framebufferConfig = glXChooseFBConfig(context.MainDisplay, screen, 0, &fbcount);
        if (!framebufferConfig) {
            zepFatal("Can't create a framebuffer for OpenGL 4.0.\n");
        } else {
            int attribs[] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
                GLX_CONTEXT_MINOR_VERSION_ARB, 0,
                GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                0
            }; 
            glcontext = glXCreateContextAttribs(context.MainDisplay, framebufferConfig[0], NULL, True, attribs);
            glXMakeCurrent(context.MainDisplay, 0, 0);
            glXDestroyContext(context.MainDisplay, tempContext);
        } 
    } else {
        glcontext = glXCreateContext(context.MainDisplay, visinfo, NULL, True);
    }

    glXMakeCurrent(context.MainDisplay, context.MainWindow, glcontext);
    
    GLenum err = glewInit();
    if (GLEW_OK != err)
        zepFatal("GLEW Error: %s\n", glewGetErrorString(err));

    // Work around some GLEW issues:    
    #define glewGetProcAddress(name) (*glXGetProcAddressARB)(name)
    glPatchParameteri = (PFNGLPATCHPARAMETERIPROC)glewGetProcAddress((const GLubyte*)"glPatchParameteri");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glewGetProcAddress((const GLubyte*)"glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)glewGetProcAddress((const GLubyte*)"glDeleteVertexArrays");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glewGetProcAddress((const GLubyte*)"glGenVertexArrays");
    glIsVertexArray = (PFNGLISVERTEXARRAYPROC)glewGetProcAddress((const GLubyte*)"glIsVertexArray");

    // Reset OpenGL error state:
    glGetError();

    zepInit();
    zepAddPath("./", ".glsl");
    zepAddPath("../", ".glsl");

    char qualifiedPath[128];
    strcpy(qualifiedPath, zepResourcePath());
    strcat(qualifiedPath, "/");
    zepAddPath(qualifiedPath, ".glsl");

    zepAddDirective("*", "#version 150");

    zepPrintString("OpenGL Version: %s\n", glGetString(GL_VERSION));
    
    ZepInitialize();
    XStoreName(context.MainDisplay, context.MainWindow, ZepGetConfig().Title);
   
    
    // -------------------
    // Start the Game Loop
    // -------------------

    unsigned int previousTime = GetMicroseconds();
    int done = 0;
    while (!done) {
        
        if (glGetError() != GL_NO_ERROR)
            zepFatal("OpenGL error.\n");

        if (XPending(context.MainDisplay)) {
            XEvent event;
    
            XNextEvent(context.MainDisplay, &event);
            switch (event.type)
            {
                case Expose:
                    //redraw(display, event.xany.window);
                    break;
                
                case ConfigureNotify:
                    //resize(event.xconfigure.width, event.xconfigure.height);
                    break;
                
#ifdef ZEP_MOUSE_HANDLER
                case ButtonPress:
                    ZepHandleMouse(event.xbutton.x, event.xbutton.y, ZEP_DOWN);
                    break;

                case ButtonRelease:
                    ZepHandleMouse(event.xbutton.x, event.xbutton.y, ZEP_UP);
                    break;

                case MotionNotify:
                    ZepHandleMouse(event.xmotion.x, event.xmotion.y, ZEP_MOVE);
                    break;
#endif

                case KeyRelease:
                case KeyPress: {
                    XComposeStatus composeStatus;
                    char asciiCode[32];
                    KeySym keySym;
                    int len;
                    
                    len = XLookupString(&event.xkey, asciiCode, sizeof(asciiCode), &keySym, &composeStatus);
                    switch (asciiCode[0]) {
                        case 'x': case 'X': case 'q': case 'Q':
                        case 0x1b:
                            done = 1;
                            break;
                    }
                }
            }
        }

        unsigned int currentTime = GetMicroseconds();
        unsigned int deltaTime = currentTime - previousTime;
        previousTime = currentTime;
        
        ZepUpdate((float) deltaTime / 1000000.0f);

        ZepRender(0);
        glXSwapBuffers(context.MainDisplay, context.MainWindow);
    }

    return 0;
}

void zepPrintStringW(const wchar_t* pStr, ...)
{
    va_list a;
    va_start(a, pStr);

    wchar_t msg[1024] = {0};
    vswprintf(msg, countof(msg), pStr, a);
    fputws(msg, stderr);
}

void zepPrintString(const char* pStr, ...)
{
    va_list a;
    va_start(a, pStr);

    char msg[1024] = {0};
    vsnprintf(msg, countof(msg), pStr, a);
    fputs(msg, stderr);
}

void zepFatalW(const wchar_t* pStr, ...)
{
    fwide(stderr, 1);

    va_list a;
    va_start(a, pStr);

    wchar_t msg[1024] = {0};
    vswprintf(msg, countof(msg), pStr, a);
    fputws(msg, stderr);
    exit(1);
}

void _zepFatal(const char* pStr, va_list a)
{
    char msg[1024] = {0};
    vsnprintf(msg, countof(msg), pStr, a);
    fputs(msg, stderr);
    exit(1);
}

void zepFatal(const char* pStr, ...)
{
    va_list a;
    va_start(a, pStr);
    _zepFatal(pStr, a);
}
void zepCheck(int condition, ...)
{
    va_list a;
    const char* pStr;

    if (condition)
        return;

    va_start(a, condition);
    pStr = va_arg(a, const char*);
    _zepFatal(pStr, a);
}

int zepIsPressing(char key)
{
    return 0;
}

const char* zepResourcePath()
{
    return ".";
}

