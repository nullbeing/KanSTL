# stlviewer

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/platform-M5Stack%20Cardputer%20Zero-orange.svg)]()

A small STL 3D viewer for the M5Stack Cardputer Zero (320x170 RGB565
framebuffer). It loads a binary or ASCII STL file, renders it with a software
rasterizer, and lets you rotate and zoom with the keyboard.

**Status: early development.** The repository currently contains the design
notes and project scaffold. Code will follow.

## Goal

- Load STL files from the home directory or an SD card
- Rotate with the arrow keys and zoom with `+` / `-`
- Solid shaded rendering with a z-buffer, wireframe toggle
- Comfortable frame rate for models up to ~12k triangles (15 fps minimum)
- Pixel-art UI using a compact bitmap font, no LVGL dependency
- Runs as the normal `pi` user under APPLaunch

## Planned controls

| Key | Action |
| --- | --- |
| Left / Right | Rotate around Y (yaw) |
| Up / Down | Rotate around X (pitch) |
| `+` / `-` | Zoom in / out |
| `R` | Reset view |
| `F` | Fit model to screen |
| `W` | Toggle wireframe / solid |
| `A` | Toggle auto-rotate |
| `Esc` | Back to file list (long press exits) |

## Target hardware

- M5Stack Cardputer Zero (CM0, aarch64), Debian 13
- 320x170 16bpp framebuffer (`/dev/fb0`, `/dev/fb_lcd`)
- TCA8418 evdev keyboard (`/dev/input/by-path/platform-3f804000.i2c-event`)

## Design notes

See [docs/DESIGN.md](docs/DESIGN.md) for the rendering pipeline, performance
budget and packaging plan.

## License

GPL-3.0. The framebuffer and input patterns are shared with
[zeroSDR](https://github.com/nullbeing/zerosdr).
