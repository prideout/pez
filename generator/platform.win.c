#pragma comment(lib,"Comdlg32.lib")

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HDC hDC;

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE ignoreMe0, LPSTR ignoreMe1, INT ignoreMe2)
{
    LPCSTR szName = "Zep App";
    WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC | CS_DBLCLKS, MsgProc, 0L, 0L, GetModuleHandle(0), 0, 0, 0, 0, szName, 0 };
    DWORD dwStyle = WS_SYSMENU | WS_VISIBLE | WS_POPUP;
    DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    RECT rect;
    int windowWidth, windowHeight, windowLeft, windowTop;
    HWND hWnd;
    PIXELFORMATDESCRIPTOR pfd;
    HGLRC hRC;
    int pixelFormat;
    GLenum err;
    MSG msg = {0};
    LARGE_INTEGER previousTime;
    LARGE_INTEGER freqTime;

    wc.hCursor = LoadCursor(0, IDC_ARROW);
    RegisterClassExA(&wc);

    SetRect(&rect, 0, 0, ZepGetConfig().Width, ZepGetConfig().Height);
    AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);
    windowWidth = rect.right - rect.left;
    windowHeight = rect.bottom - rect.top;
    windowLeft = GetSystemMetrics(SM_CXSCREEN) / 2 - windowWidth / 2;
    windowTop = GetSystemMetrics(SM_CYSCREEN) / 2 - windowHeight / 2;
    hWnd = CreateWindowExA(0, szName, szName, dwStyle, windowLeft, windowTop, windowWidth, windowHeight, 0, 0, 0, 0);

    // Create the GL context.
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;

    hDC = GetDC(hWnd);
    pixelFormat = ChoosePixelFormat(hDC, &pfd);

    SetPixelFormat(hDC, pixelFormat, &pfd);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    if (ZepGetConfig().Multisampling)
    {
        int pixelAttribs[] =
        {
            WGL_SAMPLES_ARB, 16,
            WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_RED_BITS_ARB, 8,
            WGL_GREEN_BITS_ARB, 8,
            WGL_BLUE_BITS_ARB, 8,
            WGL_ALPHA_BITS_ARB, 8,
            WGL_DEPTH_BITS_ARB, 24,
            WGL_STENCIL_BITS_ARB, 8,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            0
        };
        int* sampleCount = pixelAttribs + 1;
        int* useSampleBuffer = pixelAttribs + 3;
        int pixelFormat = -1;
        PROC proc = wglGetProcAddress("wglChoosePixelFormatARB");
        unsigned int numFormats;
        PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC) proc;

        if (!wglChoosePixelFormatARB)
        {
            zepFatal("Could not load function pointer for 'wglChoosePixelFormatARB'.  Is your driver properly installed?");
        }

        // Try fewer and fewer samples per pixel till we find one that is supported:
        while (pixelFormat <= 0 && *sampleCount >= 0)
        {
            wglChoosePixelFormatARB(hDC, pixelAttribs, 0, 1, &pixelFormat, &numFormats);
            (*sampleCount)--;
            if (*sampleCount <= 1)
            {
                *useSampleBuffer = GL_FALSE;
            }
        }

        // Win32 allows the pixel format to be set only once per app, so destroy and re-create the app:
        DestroyWindow(hWnd);
        hWnd = CreateWindowExA(0, szName, szName, dwStyle, windowLeft, windowTop, windowWidth, windowHeight, 0, 0, 0, 0);
        SetWindowPos(hWnd, HWND_TOP, windowLeft, windowTop, windowWidth, windowHeight, 0);
        hDC = GetDC(hWnd);
        SetPixelFormat(hDC, pixelFormat, &pfd);
        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);
    }

    #ifdef ZEP_DROP_HANDLER
    DragAcceptFiles(hWnd, TRUE);
    #endif

    err = glewInit();
    if (GLEW_OK != err)
    {
        zepFatal("GLEW Error: %s\n", glewGetErrorString(err));
    }
    zepPrintString("OpenGL Version: %s\n", glGetString(GL_VERSION));

    if (!ZepGetConfig().VerticalSync)
    {
        wglSwapIntervalEXT(0);
    }

    if (ZEP_FORWARD_COMPATIBLE_GL && glewIsSupported("GL_VERSION_3_2"))
    {
        const int contextAttribs[] =
        {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 2,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            0
        };

        HGLRC newRC = wglCreateContextAttribsARB(hDC, 0, contextAttribs);
        wglMakeCurrent(0, 0);
        wglDeleteContext(hRC);
        hRC = newRC;
        wglMakeCurrent(hDC, hRC);
        
        zepPrintString("Switched to a forward-compatible context.\n");
    }

    zepInit();
    zepAddPath("./", ".glsl");
    zepAddPath("../", ".glsl");

    char qualifiedPath[128];
    strcpy(qualifiedPath, zepResourcePath());
    strcat(qualifiedPath, "/");
    zepAddPath(qualifiedPath, ".glsl");

    zepAddDirective("*", "#version 150");

    ZepInitialize();
    SetWindowTextA(hWnd, ZepGetConfig().Title);

    QueryPerformanceFrequency(&freqTime);
    QueryPerformanceCounter(&previousTime);

    // -------------------
    // Start the Game Loop
    // -------------------
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            LARGE_INTEGER currentTime;
            __int64 elapsed;
            double deltaTime;

            QueryPerformanceCounter(&currentTime);
            elapsed = currentTime.QuadPart - previousTime.QuadPart;
            deltaTime = elapsed * 1000000.0 / freqTime.QuadPart;
            previousTime = currentTime;

            ZepUpdate((float) deltaTime / 1000000.0f);
            ZepRender();
            SwapBuffers(hDC);
            zepCheck(glGetError() == GL_NO_ERROR, "OpenGL error.\n");
        }
    }

    UnregisterClassA(szName, wc.hInstance);

    return 0;
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static POINT cursorBegin;
    static RECT windowRectBegin;
    int x = LOWORD(lParam);
    int y = HIWORD(lParam);
    switch (msg)
    {
        #ifdef ZEP_DROP_HANDLER
        case WM_DROPFILES:
        {
            HDROP query = (HDROP) wParam;
            int n = 0, count = DragQueryFile( query, 0xFFFFFFFF, 0, 0 );
            while ( n < count )
            {
                char file[512];
                DragQueryFile( query, n, file, 512 );
                ZepReceiveDrop(file);
                n++;
            }
            DragFinish( query );
          }
          break;
        #endif

        #ifdef ZEP_MOUSE_HANDLER
        case WM_LBUTTONDBLCLK:
            ZepHandleMouse(x, y, ZEP_DOUBLECLICK);
            break;

        case WM_LBUTTONUP:
            ZepHandleMouse(x, y, ZEP_UP);
            break;

        case WM_LBUTTONDOWN:
            ZepHandleMouse(x, y, ZEP_DOWN);
            break;
        #endif

        case WM_MBUTTONDOWN:
            cursorBegin.x = x;
            cursorBegin.y = y;
            ClientToScreen(hWnd, &cursorBegin);
            GetWindowRect(hWnd, &windowRectBegin);
            break;

        case WM_MOUSEMOVE:
        
            #ifdef ZEP_MOUSE_HANDLER
            if (wParam & MK_LBUTTON) {
                PezHandleMouse(x, y, PEZ_MOVE);
                SetCursor(LoadCursor(0, IDC_HAND));
            } else {
                SetCursor(LoadCursor(0, IDC_ARROW));
            }
            #endif
            
            if (wParam & MK_MBUTTON) {
                POINT cursorEnd = { x, y };
                ClientToScreen(hWnd, &cursorEnd);
                SHORT dx = (SHORT) (cursorEnd.x - cursorBegin.x);
                SHORT dy = (SHORT) (cursorEnd.y - cursorBegin.y);
                SHORT width = (SHORT) (windowRectBegin.right - windowRectBegin.left);
                SHORT height = (SHORT) (windowRectBegin.bottom - windowRectBegin.top);
                MoveWindow(hWnd, windowRectBegin.left + dx, windowRectBegin.top + dy, width, height, TRUE);
            }
            break;

        case WM_KEYDOWN:
        {
            switch (wParam)
            {
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    break;
                case VK_OEM_2: // Question Mark / Forward Slash for US Keyboards
                    break;
            }
            break;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int zepIsPressing(char key)
{
    return GetAsyncKeyState(key) & 0x0001;
}

const char* zepResourcePath()
{
    return "..";
}

#ifdef _MSC_VER

void zepPrintString(const char* pStr, ...)
{
    char msg[1024] = {0};

    va_list a;
    va_start(a, pStr);

    _vsnprintf_s(msg, _countof(msg), _TRUNCATE, pStr, a);
    OutputDebugStringA(msg);
}

void _zepFatal(const char* pStr, va_list a)
{
    char msg[1024] = {0};
    _vsnprintf_s(msg, _countof(msg), _TRUNCATE, pStr, a);
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
    __debugbreak();
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

#else

void zepPrintString(const char* pStr, ...)
{
    va_list a;
    va_start(a, pStr);

    char msg[1024] = {0};
    vsnprintf(msg, countof(msg), pStr, a);
    fputs(msg, stderr);
}

void _zepFatal(const char* pStr, va_list a)
{
    char msg[1024] = {0};
    vsnprintf(msg, countof(msg), pStr, a);
    fputs(msg, stderr);
    //__builtin_trap();
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

#endif

const char* zepGetDesktopFolder()
{
    HKEY hKey;
    static char lszValue[255];
    DWORD dwType=REG_SZ;
    DWORD dwSize=255;
    RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders", 0L, KEY_READ , &hKey);
    RegQueryValueEx(hKey, "Desktop", NULL, &dwType,(LPBYTE)&lszValue, &dwSize);
    return lszValue;
}

void zepSwapBuffers()
{
    SwapBuffers(hDC);
}

const char* zepOpenFileDialog()
{
    static TCHAR szFile[MAX_PATH] = TEXT("\0");
    OPENFILENAME ofn;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD dwFileSize = 0, bytesToRead = 0, bytesRead = 0;
    memset( &(ofn), 0, sizeof(ofn));
    ofn.lStructSize   = sizeof(ofn);
    ofn.hwndOwner = 0;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT("Data file (*.*)\0 *.*\0");  
    ofn.lpstrTitle = TEXT("Open File");
    ofn.Flags = OFN_EXPLORER;

    if (GetOpenFileName(&ofn))
    {
        return ofn.lpstrFile;
    }

    return 0;
}
