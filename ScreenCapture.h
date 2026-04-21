#pragma once

/**
 * ScreenCapture.h
 * FFGL 2.1 Source Plugin — grabs a window or monitor and streams it into Resolume.
 */

// ─── FFGL SDK headers ──────────────────────────────────────────────────────
// These come from the official FFGL repo:
//   https://github.com/resolume/ffgl  →  source/lib/ffgl/
#include <FFGL.h>
#include <FFGLLib.h>
#include <FFGLPluginSDK.h>

// Platform headers
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  using NativeHandle = HWND;
#else
  using NativeHandle = void*;
#endif

#include <vector>
#include <string>

// ─── Window descriptor ─────────────────────────────────────────────────────

struct WindowInfo
{
    NativeHandle hwnd;
    std::string  title;
    int          width;
    int          height;
};

// ─── Plugin class ──────────────────────────────────────────────────────────

class ScreenCapturePlugin : public CFFGLPlugin
{
public:
    ScreenCapturePlugin();
    ~ScreenCapturePlugin() override;

    // FFGL interface
    FFResult InitGL(const FFGLViewportStruct* vp) override;
    FFResult DeInitGL() override;
    FFResult Render(memoryStructGL* pGL) override;

    FFResult SetFloatParameter(unsigned int index, float value) override;
    float    GetFloatParameter(unsigned int index) override;
    char*    GetParameterDisplay(DWORD index) override;

    static CFFGLPlugin* CreateInstance() { return new ScreenCapturePlugin(); }

private:
    // ── Capture ──────────────────────────────────────────────────────────
    void         RefreshWindowList();
    NativeHandle GetSelectedWindow();
    bool         CaptureWindow(NativeHandle hwnd);
    bool         CaptureMonitor(int monitorIndex);

    // ── OpenGL ────────────────────────────────────────────────────────────
    void DrawFullscreenQuad();
    void ApplyBrightnessContrast();

    // ── State ─────────────────────────────────────────────────────────────
    GLuint                  m_texture;
    unsigned char*          m_pixelBuffer;
    int                     m_bufferWidth;
    int                     m_bufferHeight;
    std::vector<WindowInfo> m_windows;
    NativeHandle            m_selectedHwnd;
    uint64_t                m_frameCount;

    // ── Parameters ────────────────────────────────────────────────────────
    float m_windowIndex;    // [0..1] maps to window list index
    float m_monitorIndex;   // [0..1] maps to monitor index
    float m_captureCursor;  // boolean
    float m_flipV;          // boolean – flip vertically (GDI bitmaps are bottom-up)
    float m_brightness;     // [0..1] center = 0.5 (no change)
    float m_contrast;       // [0..1] center = 0.25 (no change, maps to ×1.0)
};
