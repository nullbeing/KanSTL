# stlviewer design notes

## Implementation status

- M1 complete: STL loader, wireframe renderer, keyboard rotation/zoom, pixel UI,
  offscreen PNG rendering, benchmark mode
- M2 complete: z-buffer solid rendering, flat shading, backface culling,
  wireframe mode, solid mode is the default
- Device measurement (CM0, 12,000 triangles): solid 7.8 ms/frame (~128 fps),
  wireframe 12.6 ms/frame (~79 fps); interactive rendering is paced at ~60 fps
- M3 complete: in-app file browser, remembered last directory, file sizes,
  loading frame and load-error messages
- M4 in progress: EDGES (feature + silhouette lines), MATCAP shading and a
  render-style pill; decimation, quality modes and packaging assets remain
- Device measurement (23,776 triangles): edges 17.8 ms/frame (~56 fps),
  matcap 10.1 ms/frame (~99 fps)

## Scope

An interactive STL viewer for the M5Stack Cardputer Zero.

Agreed constraints:

- Model size: up to ~12,000 triangles is enough
- Frame rate: 15 fps acceptable, aim higher when possible
- Formats: STL only for now (binary + ASCII)
- UI: pixel-art style with a compact bitmap font, same direction as zeroSDR
- AppStore: later; build a GitHub project and a working device app first

## Hardware baseline

| Item | Value |
| --- | --- |
| SoC | M5Stack Cardputer Zero (CM0), 4 x aarch64 |
| Display | 320x170, RGB565, `/dev/fb0` (`/dev/fb_lcd`) |
| Framebuffer | line_length 640, one frame = 108,800 bytes |
| Input | TCA8418 evdev keyboard, no touch |
| Runtime | APPLaunch systemd user service, user `pi` |
| Graphics | no X/Wayland or GL context for apps; direct fbdev only |

The screen blanks on idle. A full-screen app must reset the backlight and
`FBIOBLANK` about once per second.

## Chosen approach

Custom software rasterizer writing RGB565 directly to the framebuffer.

Rejected alternatives:

- GPU / DRM / EGL: conflicts with APPLaunch over DRM master, uncertain driver
  support, much higher debugging cost for almost no benefit at 320x170.
- TinyGL: workable, but adds fixed-point quirks and GPL licensing questions.
- LVGL canvas: useful for complex UI, but no stable 3D/z-buffer API and extra
  per-frame copies. Keep the renderer UI-independent so LVGL can be added later.

## Pipeline

```text
STL file
  -> triangle soup (positions + face normals)
  -> normalize (AABB center, scale to fit)
  -> camera (rotation + orthographic projection, optional perspective)
  -> near-plane clip
  -> backface cull (signed area after projection)
  -> rasterize (edge functions + z-buffer)
  -> shade (flat + two lights, optional matcap LUT)
  -> RGB565 back buffer
  -> blit to /dev/fb0
```

## Modules

```text
include/stl_loader.h      parse binary/ASCII STL, fix winding, normalize
include/scene.h           yaw/pitch/roll, zoom, camera matrices
include/rasterizer.h      z-buffer, triangle fill, shading
include/render_target.h   RGB565 buffer, optional low-res upscale
include/ui.h              status bar, help overlay, file list
include/input.h           evdev keyboard handling
src/...                   implementations
```

The renderer has no dependency on the UI or on APPLaunch.

## Rendering details

- z-buffer: 320 x 170 floats = 217 KB, no memory pressure
- Backface culling first: removes roughly half the triangles
- Rotation center (`pivot`) is stored in normalized model space and subtracted
  before rotation; Alt+arrows move it along the current screen axes and a
  crosshair marks the orbit point
- Flat shading by default; STL is faceted anyway and it is the cheapest mode
- Two-sided shading (`abs(dot)`) tolerates inconsistent STL normals
- EDGES: flat fill plus dark crease lines (40 degree threshold) and a bright
  depth-tested silhouette
- MATCAP: 32x32 normal lookup with a specular highlight and rim term
- Ordered 4x4 dithering is a future option to hide RGB565 banding
- Quality modes: native 320x170, or 160x85 rendered and scaled 2x
- Render on demand: skip frames when the view is static and auto-rotate is off

## Performance budget

Conservative estimate for 1 GHz Cortex-A53, 12k triangles, single core:

```text
vertex transform   36k vertices  ~ 2 ms
triangle setup     12k triangles ~ 1.2 ms
raster fill        ~30k pixels   ~ 0.3 ms
total                            ~ 3.5-5 ms  -> well above 30 fps
```

Planned optimizations, in order:

1. Backface and frustum culling
2. Lower internal resolution when interacting
3. Split the screen into horizontal bands across 4 threads
4. Vertex clustering decimation for oversized models
5. `-O3` and NEON-friendly math; avoid per-pixel floating point in the inner loop

## STL loading

- Binary layout: 80-byte header + `uint32` count + 50 bytes per triangle.
  Detect by file size `84 + 50*n`, not by the `solid` prefix.
- ASCII STL fallback: slower; parse in a background thread with progress.
- `mmap` large binary files and reference triangles directly.
- Normalize at load: center AABB, scale to fit, recompute degenerate normals,
  fix global winding using signed volume.
- Limits: warn above 64 MB or 2 million triangles.

## Interaction

```text
Left/Right   yaw
Up/Down      pitch
Alt+arrows   move the rotation center along screen axes
Enter        zoom in
Space        zoom out
R            reset view and center
0            fit projected bounds to the viewport and recenter
7            cycle solid / edges / matcap / wireframe
6            cycle auto-rotate off / X pitch / Y yaw / Z roll
8            open the file list (under the on-screen pill)
Esc (3s)     hold to exit; an on-screen progress prompt is shown
```

The viewer shows a red/green/blue XYZ orientation gizmo in the top-right
corner of the 3D viewport, with no frame or arrowheads, drawn above the model
and over the UI overlays.

The bottom bar has three pills: an auto-rotate toggle at x=161 (`auto`,
`auto x`, `auto y`, `auto z`; mint green when running) under the `6` key, a render-style button at x=217
(`solid`, `edges`, `matcap` or `wire`) under the `7` key, and a `files` button
at x=273 under the `8` key. `8` opens the file browser.

Status bar: file name, triangle count, fps. The help overlay is
a rounded, shadowed panel; keys are color-coded by group (cyan for view
movement, amber for display/file actions, red for exit).

File browser (first start opens `$HOME`, then the last used directory):

```text
Up/Down      select
PgUp/PgDn    page
Enter        open folder or model
Left/Backspace  parent folder
Esc (3s)     hold to exit, with the same progress prompt
R            reload
```

It lists folders and `.stl` files only, shows file sizes, remembers the last
directory and selection in `~/.config/stlviewer/last_dir`, and displays a
loading frame plus load errors without leaving the browser.

## Packaging (later)

- APPLaunch entry with `Terminal=false` and a wrapper under `bin/`
- App-specific icon and four 320x170 screenshots
- Everything under `/usr/share/APPLaunch`
- No root, no network permissions

## Roadmap

```text
M1  STL parser + wireframe + rotate/zoom
M2  z-buffer solid rendering + flat shading + culling
M3  file browser, remembered directory, loading/error states
M4  quality modes, decimation, matcap, packaging assets
M5  device test, AppStore submission
```
