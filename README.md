# ScreenCapture — FFGL Plugin for Resolume Avenue / Arena

Captures any desktop **window** or **monitor** and streams it as a live video
source inside Resolume Avenue or Arena. Works like a virtual camera, but zero
latency and no extra software required.

---

## Features

| Feature | Detail |
|---|---|
| Window capture | Any visible, titled window (browser, game, app, DAW…) |
| Monitor capture | Full display / multi-monitor support |
| Cursor overlay | Optionally draw the mouse pointer into the frame |
| Flip vertical | Compensate for GDI's bottom-up bitmap orientation |
| Brightness | Live tweak in Resolume |
| Contrast | Live tweak in Resolume |
| FFGL 2.1 | Works with Resolume Avenue & Arena 7+ |

---

## Requirements

| Tool | Version |
|---|---|
| Windows | 10 / 11 (64-bit) |
| Visual Studio | 2019 or 2022 (Community is fine) |
| CMake | 3.18 + |
| FFGL SDK | Latest from GitHub (see below) |
| Resolume | Avenue or Arena 7+ |

> **macOS**: A stub is included. Full macOS support requires replacing the
> capture backend with `ScreenCaptureKit` (macOS 12.3+) or
> `CGWindowListCreateImage`. Pull requests welcome.

---

## 1 · Get the FFGL SDK

```bash
git clone https://github.com/resolume/ffgl.git
```

Note the path — you will pass it to CMake as `FFGL_ROOT`.

---

## 2 · Build

```bash
# From inside the resolume-screencap folder:
mkdir build && cd build

cmake .. \
  -DFFGL_ROOT="C:/dev/ffgl" \
  -DCMAKE_BUILD_TYPE=Release \
  -A x64

cmake --build . --config Release
```

The output file is `build/Release/ScreenCapture.dll`.

---

## 3 · Install in Resolume

1. Close Resolume.
2. Copy `ScreenCapture.dll` to:
   ```
   %PROGRAMDATA%\Resolume Avenue\vfx\
   ```
   or for Arena:
   ```
   %PROGRAMDATA%\Resolume Arena\vfx\
   ```
3. Launch Resolume.
4. In the **Sources** panel, search for **"Screen Capture"**.
5. Drag it onto a layer.

Alternatively, run the CMake install step which copies it automatically:

```bash
cmake --install . --config Release
```

---

## 4 · Parameters

| Parameter | Type | Description |
|---|---|---|
| **Window** | Slider | Scrolls through visible windows. Display shows the window title. |
| **Monitor** | Slider | Selects a monitor (0 = primary). |
| **Capture Cursor** | Toggle | Draw the mouse pointer into the output. |
| **Flip Vertical** | Toggle | Flip the image upside-down (on by default — GDI bitmaps are bottom-up). |
| **Brightness** | Slider | 0.5 = unchanged. Lower = darker, higher = brighter. |
| **Contrast** | Slider | 0.25 = unchanged (maps to ×1.0 scale). |

> **Tip**: Use the **Window** slider and watch the parameter display to find
> the window you want — it shows the window title in real time.

---

## 5 · Performance notes

- The plugin uses **GDI `PrintWindow`** (with `PW_RENDERFULLCONTENT`) which
  works on DWM-composited windows including hardware-accelerated apps and
  browsers. For games running in exclusive fullscreen, use **Monitor** capture
  instead.
- For maximum frame rate, run Resolume at 30 fps or lower if you are also
  streaming at high quality.
- The plugin does **not** use DXGI Desktop Duplication (which would require
  D3D11 and additional complexity). If you need sub-frame latency, that is the
  next upgrade path.

---

## 6 · Upgrading to DXGI Desktop Duplication (advanced)

For near-zero-latency full-screen capture, replace `CaptureMonitor()` with the
DXGI Desktop Duplication API:

```cpp
// Sketch only — see Microsoft docs for full example
IDXGIOutputDuplication* duplication;
output->DuplicateOutput(d3dDevice, &duplication);
duplication->AcquireNextFrame(timeout, &frameInfo, &desktopResource);
// Map the ID3D11Texture2D, copy to CPU, upload to GL texture
```

This avoids the GDI round-trip and can hit 60 fps reliably.

---

## 7 · Troubleshooting

| Symptom | Fix |
|---|---|
| Plugin not visible in Resolume | Make sure the `.dll` is in the correct `vfx` folder and Resolume was restarted. |
| Black output | The selected window may be minimized or off-screen. Restore it. |
| Upside-down image | Toggle **Flip Vertical** on. |
| Flickering | Some DRM-protected windows (Netflix, etc.) block `PrintWindow`. Switch to Monitor capture. |
| Wrong window selected | The **Window** slider is a 0–1 range over all visible titled windows enumerated at startup. Re-open Resolume to refresh the list. |

---

## License

MIT — see `LICENSE.txt`

## Credits

Built on the [FFGL SDK](https://github.com/resolume/ffgl) by Resolume.
