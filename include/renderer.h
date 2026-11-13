#ifndef KANSTL_RENDERER_H
#define KANSTL_RENDERER_H

#include <cstdint>
#include <vector>

#include "canvas.h"
#include "stl_loader.h"

struct Camera {
    float yaw_deg = -30.0f;
    float pitch_deg = 20.0f;
    float roll_deg = 0.0f;
    float zoom = 1.0f;
    // Rotation center in normalized model space. (0,0,0) is the model center.
    float pivot_x = 0.0f;
    float pivot_y = 0.0f;
    float pivot_z = 0.0f;
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

enum class RenderMode {
    Solid,
    Edges,
    Matcap,
    Wireframe,
};

class Renderer {
public:
    Renderer();

    void render(const Mesh& mesh, const Camera& camera, Canvas& canvas,
                const Rect& viewport, RenderMode mode);

private:
    void project(const Mesh& mesh, const Camera& camera, const Rect& viewport);
    void renderWireframe(Canvas& canvas, const Rect& viewport);
    void renderSolid(const Mesh& mesh, Canvas& canvas, const Rect& viewport,
                     float cp, float sp, float cy, float sy, float cr,
                     float sr, bool matcap);
    void renderFeatureEdges(const Mesh& mesh, const Camera& camera,
                            Canvas& canvas, const Rect& viewport, float cp,
                            float sp, float cy, float sy);

    void buildMatcap();
    uint16_t matcapColor(float nx, float ny) const;

    std::vector<float> screen_x_;
    std::vector<float> screen_y_;
    std::vector<float> depth_;
    std::vector<float> zbuffer_;
    std::vector<float> edge_screen_x_;
    std::vector<float> edge_screen_y_;
    std::vector<float> edge_depth_;
    uint16_t matcap_[32 * 32] = {};
};

#endif  // KANSTL_RENDERER_H
