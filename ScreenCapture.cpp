/**
 * ScreenCapture FFGL Plugin for Resolume Avenue/Arena
 *
 * Captures a selected window or monitor region and outputs it as
 * an OpenGL texture that Resolume can use as a video source.
 *
 * Platform: Windows (primary), macOS (secondary)
 * FFGL Version: 2.1
 *
 * Build dependencies:
 *   - FFGL SDK (https://github.com/resolume/ffgl)
 *   - OpenGL / GLEW
 *   - Windows: Desktop Duplication API (DXGI) or GDI BitBlt
 *   - macOS:   ScreenCaptureKit or CGWindowListCreateImage
 */

#include "ScreenCapture.h"
#include <cstring>
#include <algorithm>
#include <sstream>

// ─── Static plugin info ────────────────────────────────────────────────────

static CFFGLPluginInfo PluginInfo(
    ScreenCapturePlugin::CreateInstance,
    "SCAP",                          // 4-char plugin ID (unique)
    "Screen Capture",                // Human-readable name
    2,                               // FFGL major version
    1,                               // FFGL minor version
    1,                               // Plugin major version
    0,                               // Plugin minor version
    FF_SOURCE,                       // Plugin type: source (generates video)
    "Captures a desktop window or monitor and outputs it as a video source.",
    "https://github.com/yourname/resolume-screencap"
);

// ─── Parameter indices ─────────────────────────────────────────────────────

enum Params {
    PARAM_WINDOW_INDEX  = 0,
    PARAM_MONITOR_INDEX = 1,
    PARAM_CAPTURE_CURSOR= 2,
    PARAM_FLIP_V        = 3,
    PARAM_BRIGHTNESS    = 4,
    PARAM_CONTRAST      = 5,
    PARAM_COUNT
};

// ─── Constructor / Destructor ──────────────────────────────────────────────

ScreenCapturePlugin::ScreenCapturePlugin()
    : CFFGLPlugin()
    , m_texture(0)
    , m_pixelBuffer(nullptr)
    , m_bufferWidth(0)
    , m_bufferHeight(0)
    , m_windowIndex(0.0f)
    , m_monitorIndex(0.0f)
    , m_captureCursor(1.0f)
    , m_flipV(1.0f)
    , m_brightness(0.5f)
    , m_contrast(0.5f)
    , m_selectedHwnd(nullptr)
    , m_frameCount(0)
{
    // Declare parameters
    SetMinInputs(0);
    SetMaxInputs(0);

    AddParam(CFFGLParameterDef("Window",         FF_TYPE_OPTION, 0.0f));
    AddParam(CFFGLParameterDef("Monitor",        FF_TYPE_OPTION, 0.0f));
    AddParam(CFFGLParameterDef("Capture Cursor", FF_TYPE_BOOLEAN, 1.0f));
    AddParam(CFFGLParameterDef("Flip Vertical",  FF_TYPE_BOOLEAN, 1.0f));
    AddParam(CFFGLParameterDef("Brightness",     FF_TYPE_STANDARD, 0.5f));
    AddParam(CFFGLParameterDef("Contrast",       FF_TYPE_STANDARD, 0.5f));

    RefreshWindowList();
}

ScreenCapturePlugin::~ScreenCapturePlugin()
{
    delete[] m_pixelBuffer;
}

// ─── Plugin lifecycle ──────────────────────────────────────────────────────

FFResult ScreenCapturePlugin::InitGL(const FFGLViewportStruct* vp)
{
    // Generate an OpenGL texture to upload captured frames into
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return FF_SUCCESS;
}

FFResult ScreenCapturePlugin::DeInitGL()
{
    if (m_texture)
    {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    return FF_SUCCESS;
}

// ─── Main render call ──────────────────────────────────────────────────────

FFResult ScreenCapturePlugin::Render(memoryStructGL* pGL)
{
    // Select source: window or full monitor
    HWND hwnd = GetSelectedWindow();
    bool captured = false;

    if (hwnd && IsWindow(hwnd))
        captured = CaptureWindow(hwnd);
    else
        captured = CaptureMonitor(static_cast<int>(m_monitorIndex * 8.0f));

    if (!captured || !m_pixelBuffer)
        return FF_SUCCESS; // Render nothing this frame

    // Upload pixel data to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 m_bufferWidth, m_bufferHeight, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, m_pixelBuffer);

    // Fullscreen quad with adjustments
    glEnable(GL_TEXTURE_2D);
    ApplyBrightnessContrast();
    DrawFullscreenQuad();
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    ++m_frameCount;
    return FF_SUCCESS;
}

// ─── Windows capture (GDI BitBlt) ─────────────────────────────────────────

#ifdef _WIN32

void ScreenCapturePlugin::RefreshWindowList()
{
    m_windows.clear();
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* list = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
        if (!IsWindowVisible(hwnd)) return TRUE;

        char title[256] = {0};
        GetWindowTextA(hwnd, title, sizeof(title));
        if (strlen(title) == 0) return TRUE;

        RECT r;
        GetWindowRect(hwnd, &r);
        int w = r.right  - r.left;
        int h = r.bottom - r.top;
        if (w < 10 || h < 10) return TRUE;

        WindowInfo info;
        info.hwnd  = hwnd;
        info.title = title;
        info.width = w;
        info.height = h;
        list->push_back(info);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&m_windows));
}

HWND ScreenCapturePlugin::GetSelectedWindow()
{
    int idx = static_cast<int>(m_windowIndex * std::max(1.0f, (float)(m_windows.size() - 1)));
    idx = std::clamp(idx, 0, (int)m_windows.size() - 1);
    return m_windows.empty() ? nullptr : m_windows[idx].hwnd;
}

bool ScreenCapturePlugin::CaptureWindow(HWND hwnd)
{
    RECT r;
    if (!GetWindowRect(hwnd, &r)) return false;

    int w = r.right  - r.left;
    int h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return false;

    // Allocate or reallocate buffer
    if (w != m_bufferWidth || h != m_bufferHeight)
    {
        delete[] m_pixelBuffer;
        m_bufferWidth  = w;
        m_bufferHeight = h;
        m_pixelBuffer  = new BYTE[w * h * 4];
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = m_flipV > 0.5f ? -h : h; // negative = top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBmp) { DeleteDC(hdcMem); ReleaseDC(nullptr, hdcScreen); return false; }

    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);

    // Use PrintWindow for DWM-composited windows (more reliable than BitBlt)
    BOOL ok = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);
    if (!ok)
        BitBlt(hdcMem, 0, 0, w, h, hdcScreen, r.left, r.top, SRCCOPY);

    // Draw cursor if requested
    if (m_captureCursor > 0.5f)
    {
        CURSORINFO ci = { sizeof(CURSORINFO) };
        if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING))
        {
            ICONINFO ii = {};
            if (GetIconInfo(ci.hCursor, &ii))
            {
                int cx = ci.ptScreenPos.x - r.left - (int)ii.xHotspot;
                int cy = ci.ptScreenPos.y - r.top  - (int)ii.yHotspot;
                DrawIconEx(hdcMem, cx, cy, ci.hCursor, 0, 0, 0, nullptr, DI_NORMAL);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
                if (ii.hbmMask)  DeleteObject(ii.hbmMask);
            }
        }
    }

    GdiFlush();
    memcpy(m_pixelBuffer, pBits, w * h * 4);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return true;
}

bool ScreenCapturePlugin::CaptureMonitor(int monitorIdx)
{
    // Enumerate monitors and pick by index
    struct MonitorEnum { int target, current; RECT rect; bool found; };
    MonitorEnum me = { monitorIdx, 0, {}, false };

    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR hMon, HDC, LPRECT lprc, LPARAM lp) -> BOOL {
            auto* me = reinterpret_cast<MonitorEnum*>(lp);
            if (me->current == me->target) {
                me->rect  = *lprc;
                me->found = true;
                return FALSE;
            }
            ++me->current;
            return TRUE;
        }, reinterpret_cast<LPARAM>(&me));

    if (!me.found)
    {
        // Fall back to primary monitor
        me.rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    }

    int w = me.rect.right  - me.rect.left;
    int h = me.rect.bottom - me.rect.top;
    if (w <= 0 || h <= 0) return false;

    if (w != m_bufferWidth || h != m_bufferHeight)
    {
        delete[] m_pixelBuffer;
        m_bufferWidth  = w;
        m_bufferHeight = h;
        m_pixelBuffer  = new BYTE[w * h * 4];
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = m_flipV > 0.5f ? -h : h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);

    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, me.rect.left, me.rect.top, SRCCOPY);
    GdiFlush();
    memcpy(m_pixelBuffer, pBits, w * h * 4);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return true;
}

#else // ── macOS stub ──────────────────────────────────────────────────────

void ScreenCapturePlugin::RefreshWindowList()
{
    // macOS: use CGWindowListCopyWindowInfo
    // Implementation requires linking CoreGraphics
    // See: CGWindowListCreateImage in <CoreGraphics/CoreGraphics.h>
    m_windows.clear();
    WindowInfo info;
    info.hwnd  = (void*)1;
    info.title = "Primary Display";
    info.width = 1920;
    info.height = 1080;
    m_windows.push_back(info);
}

void* ScreenCapturePlugin::GetSelectedWindow()
{
    return m_windows.empty() ? nullptr : m_windows[0].hwnd;
}

bool ScreenCapturePlugin::CaptureWindow(void* /*hwnd*/)
{
    // macOS: CGWindowListCreateImageFromArray or ScreenCaptureKit
    return false;
}

bool ScreenCapturePlugin::CaptureMonitor(int /*idx*/)
{
    return false;
}

#endif

// ─── OpenGL helpers ────────────────────────────────────────────────────────

void ScreenCapturePlugin::DrawFullscreenQuad()
{
    float flipY0 = (m_flipV > 0.5f) ? 1.0f : 0.0f;
    float flipY1 = (m_flipV > 0.5f) ? 0.0f : 1.0f;

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, flipY0); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, flipY0); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, flipY1); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, flipY1); glVertex2f(-1.0f,  1.0f);
    glEnd();
}

void ScreenCapturePlugin::ApplyBrightnessContrast()
{
    // Map [0..1] brightness param → [-1..+1] offset
    float bright = (m_brightness - 0.5f) * 2.0f;
    // Map [0..1] contrast param → [0..4] scale
    float contrast = m_contrast * 4.0f;

    glMatrixMode(GL_COLOR);
    glLoadIdentity();

    float mat[16] = {
        contrast, 0, 0, 0,
        0, contrast, 0, 0,
        0, 0, contrast, 0,
        0, 0, 0, 1
    };
    glLoadMatrixf(mat);

    // Add brightness via color offset
    float offset = bright * (1.0f - contrast * 0.25f);
    glPixelTransferf(GL_RED_BIAS,   offset);
    glPixelTransferf(GL_GREEN_BIAS, offset);
    glPixelTransferf(GL_BLUE_BIAS,  offset);

    glMatrixMode(GL_MODELVIEW);
}

// ─── Parameter accessors ───────────────────────────────────────────────────

FFResult ScreenCapturePlugin::SetFloatParameter(unsigned int index, float value)
{
    switch (index)
    {
    case PARAM_WINDOW_INDEX:  m_windowIndex  = value; return FF_SUCCESS;
    case PARAM_MONITOR_INDEX: m_monitorIndex = value; return FF_SUCCESS;
    case PARAM_CAPTURE_CURSOR:m_captureCursor= value; return FF_SUCCESS;
    case PARAM_FLIP_V:        m_flipV        = value; return FF_SUCCESS;
    case PARAM_BRIGHTNESS:    m_brightness   = value; return FF_SUCCESS;
    case PARAM_CONTRAST:      m_contrast     = value; return FF_SUCCESS;
    }
    return FF_FAIL;
}

float ScreenCapturePlugin::GetFloatParameter(unsigned int index)
{
    switch (index)
    {
    case PARAM_WINDOW_INDEX:   return m_windowIndex;
    case PARAM_MONITOR_INDEX:  return m_monitorIndex;
    case PARAM_CAPTURE_CURSOR: return m_captureCursor;
    case PARAM_FLIP_V:         return m_flipV;
    case PARAM_BRIGHTNESS:     return m_brightness;
    case PARAM_CONTRAST:       return m_contrast;
    }
    return 0.0f;
}

// Override for text param — return window list option names
char* ScreenCapturePlugin::GetParameterDisplay(DWORD index)
{
    static char buf[256];
    if (index == PARAM_WINDOW_INDEX && !m_windows.empty())
    {
        int idx = static_cast<int>(m_windowIndex * std::max(1.0f, (float)(m_windows.size()-1)));
        idx = std::clamp(idx, 0, (int)m_windows.size() - 1);
        strncpy(buf, m_windows[idx].title.c_str(), 255);
        buf[255] = '\0';
        return buf;
    }
    if (index == PARAM_MONITOR_INDEX)
    {
        int mon = static_cast<int>(m_monitorIndex * 8);
        snprintf(buf, sizeof(buf), "Monitor %d", mon);
        return buf;
    }
    return nullptr;
}
