#ifndef STLVIEWER_UI_H
#define STLVIEWER_UI_H

#include <string>

#include "canvas.h"
#include "renderer.h"

struct UiState {
    const Mesh* mesh = nullptr;
    Camera camera;
    int fps = 0;
    bool auto_rotate = false;
    bool help = false;
    std::string toast;
};

int statusBarHeight();
int hintBarHeight();
Rect viewportRect(int canvas_width, int canvas_height);
void drawUi(Canvas& canvas, const UiState& state);

#endif  // STLVIEWER_UI_H
