#ifndef STLVIEWER_UI_H
#define STLVIEWER_UI_H

#include <string>

#include "canvas.h"
#include "file_browser.h"
#include "renderer.h"

struct UiState {
    const Mesh* mesh = nullptr;
    Camera camera;
    int fps = 0;
    bool auto_rotate = false;
    bool wireframe = false;
    bool help = false;
    std::string toast;
};

int statusBarHeight();
int hintBarHeight();
Rect viewportRect(int canvas_width, int canvas_height);
void drawUi(Canvas& canvas, const UiState& state);
void drawBrowser(Canvas& canvas, const FileBrowser& browser);
void drawLoading(Canvas& canvas, const std::string& filename);
void drawExitPrompt(Canvas& canvas, float progress);
void drawAxisGizmo(Canvas& canvas, const Camera& camera, const Rect& viewport);

#endif  // STLVIEWER_UI_H
