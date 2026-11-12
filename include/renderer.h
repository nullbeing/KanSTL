#ifndef STLVIEWER_RENDERER_H
#define STLVIEWER_RENDERER_H

#include <vector>

#include "canvas.h"
#include "stl_loader.h"

struct Camera {
    float yaw_deg = -30.0f;
    float pitch_deg = 20.0f;
    float zoom = 1.0f;
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// Wireframe renderer for M1. Solid shading / z-buffer comes in M2.
class WireRenderer {
public:
    void render(const Mesh& mesh, const Camera& camera, Canvas& canvas,
                const Rect& viewport);

private:
    std::vector<float> screen_x_;
    std::vector<float> screen_y_;
    std::vector<float> depth_;
};

#endif  // STLVIEWER_RENDERER_H
