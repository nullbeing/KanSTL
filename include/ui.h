#ifndef KANSTL_UI_H
#define KANSTL_UI_H

#include <string>

#include "canvas.h"
#include "file_browser.h"
#include "renderer.h"

enum class AutoRotateMode {
    Off,
    X,
    Y,
    Z,
};

struct UiState {
    const Mesh* mesh = nullptr;
    Camera camera;
    int fps = 0;
    AutoRotateMode auto_mode = AutoRotateMode::Off;
    RenderMode style = RenderMode::Solid;
    bool help = false;
    std::string toast;
};

int statusBarHeight();
int hintBarHeight();
Rect viewportRect(int canvas_width, int canvas_height);
void drawUi(Canvas& canvas, const UiState& state);
void drawBrowser(Canvas& canvas, const FileBrowser& browser);
void drawLoadingPopup(Canvas& canvas, const std::string& filename,
                      float progress);
void drawExitPrompt(Canvas& canvas, float progress);

// Small 20x14 camera glyph used as the screenshot confirmation flash.
constexpr int kCameraIconWidth = 20;
constexpr int kCameraIconHeight = 14;
void drawCameraIcon(Canvas& canvas, int x, int y, uint16_t color);
void drawAxisGizmo(Canvas& canvas, const Camera& camera, const Rect& viewport);

#endif  // KANSTL_UI_H
