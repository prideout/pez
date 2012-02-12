#include "Utility.h"

#include <string>
using namespace std;

#ifdef WIN32
#include <windows.h>
#include <gdiplus.h>

using namespace Gdiplus;

struct OverlayContext
{
    string PreviousMessage;
    Bitmap* GdiBitmap;
    TexturePod MessageTexture;
};

static OverlayContext oc;

extern "C" {
extern HDC hDC;
}

void InitializeGdi()
{
    PezConfig cfg = PezGetConfig();

    // Create a new text context if it doesn't already exist:
    static bool first = true;
    if (first) {
        first = false;

        GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, 0);

        oc.GdiBitmap = new Bitmap(cfg.Width, cfg.Height, PixelFormat32bppARGB);
        glGenTextures(1, &oc.MessageTexture.Handle);

        oc.MessageTexture.Width = cfg.Width;
        oc.MessageTexture.Height = cfg.Height;
    }
}

TexturePod OverlayText(const char* pMessage)
{
    InitializeGdi();
    PezConfig cfg = PezGetConfig();

    // Skip GDI text generation if the string is unchanged:
    std::string message ( pMessage );
    if (message == oc.PreviousMessage)
        return oc.MessageTexture;

    oc.PreviousMessage = message;

    // Create the GDI+ drawing context and set it up:
    Graphics* gfx = Graphics::FromImage(oc.GdiBitmap);
    gfx->Clear(Color::Transparent);
    gfx->SetSmoothingMode(SmoothingModeAntiAlias);
    gfx->SetInterpolationMode(InterpolationModeHighQualityBicubic);

    // Select a font:
    FontFamily fontFamily(L"Trebuchet MS");
    const float fontSize = 16;
    PointF origin(10.0f, 10.0f);
    StringFormat format(StringAlignmentNear);

    // Create a path along the outline of the glyphs:
    GraphicsPath path;
    path.AddString(
        wstring(message.begin(), message.end()).c_str(),
        -1,
        &fontFamily,
        FontStyleRegular,
        fontSize,
        origin,
        &format);

    // Draw some glow to steer clear of crappy AA:
    for (float width = 0; width < 3; ++width) {
        Pen pen(Color(64, 0, 0, 0), width);
        pen.SetLineJoin(LineJoinRound);
        gfx->DrawPath(&pen, &path);
    }

    // Fill the glyphs:
    SolidBrush brush(Color::White);
    gfx->FillPath(&brush, &path);

    // Lock the raw pixel data and pass it to OpenGL:
    BitmapData data;
    oc.GdiBitmap->LockBits(0, ImageLockModeRead, PixelFormat32bppARGB, &data);
    pezCheck(data.Stride == sizeof(unsigned int) * cfg.Width, "Bad alignment");
    glBindTexture(GL_TEXTURE_2D, oc.MessageTexture.Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cfg.Width, cfg.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.Scan0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    oc.GdiBitmap->UnlockBits(&data);

    return oc.MessageTexture;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	using namespace Gdiplus;
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if(size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for(UINT j = 0; j < num; ++j)
	{
		if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}    
	}

	free(pImageCodecInfo);
	return 0;
}

void ExportScreenshot(const char* filename)
{
    InitializeGdi();
    PezConfig cfg = PezGetConfig();
    int Width = cfg.Width;
    int Height = cfg.Height;
    HDC memdc = CreateCompatibleDC(hDC);
    HBITMAP membit = CreateCompatibleBitmap(hDC, Width, Height);
    HBITMAP hOldBitmap = (HBITMAP) SelectObject(memdc, membit);
    BitBlt(memdc, 0, 0, Width, Height, hDC, 0, 0, SRCCOPY);
    Gdiplus::Bitmap bitmap(membit, NULL);
    CLSID clsid;
    GetEncoderClsid(L"image/png", &clsid);
    wstring wfilename(filename, filename + strlen(filename));
    bitmap.Save(wfilename.c_str(), &clsid, 0);
    SelectObject(memdc, hOldBitmap);
    DeleteObject(memdc);
    DeleteObject(membit);
}

#else

struct OverlayContext
{
    string PreviousMessage;
    TexturePod MessageTexture;
};

static OverlayContext oc;

TexturePod OverlayText(const char* message)
{
    static bool first = true;
    if (first) {
        first = false;
        glGenTextures(1, &oc.MessageTexture.Handle);
        oc.MessageTexture.Width = cfg.Width;
        oc.MessageTexture.Height = cfg.Height;
    }

    return oc.MessageTexture;
}

void ExportScreenshot(const char* filename)
{
}

#endif