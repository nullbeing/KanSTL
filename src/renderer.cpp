#include "renderer.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

}  // namespace

void WireRenderer::render(const Mesh& mesh, const Camera& camera,
                          Canvas& canvas, const Rect& viewport) {
    const size_t vertex_count = mesh.vertices.size();
    if (vertex_count == 0 || viewport.w <= 0 || viewport.h <= 0) return;

    screen_x_.resize(vertex_count);
    screen_y_.resize(vertex_count);
    depth_.resize(vertex_count);

    const float yaw = camera.yaw_deg * kDegToRad;
    const float pitch = camera.pitch_deg * kDegToRad;
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);

    const float base_scale =
        0.5f * static_cast<float>(std::min(viewport.w, viewport.h)) * 0.92f;
    const float scale = base_scale * camera.zoom;
    const float center_x = viewport.x + viewport.w * 0.5f;
    const float center_y = viewport.y + viewport.h * 0.5f;

    for (size_t i = 0; i < vertex_count; ++i) {
        const Vec3& v = mesh.vertices[i];

        // Pitch around X, then yaw around Y.
        const float y1 = v.y * cp - v.z * sp;
        const float z1 = v.y * sp + v.z * cp;
        const float x2 = v.x * cy + z1 * sy;
        const float z2 = -v.x * sy + z1 * cy;

        screen_x_[i] = center_x + x2 * scale;
        screen_y_[i] = center_y - y1 * scale;
        depth_[i] = z2;
    }

    const int min_x = viewport.x;
    const int min_y = viewport.y;
    const int max_x = viewport.x + viewport.w - 1;
    const int max_y = viewport.y + viewport.h - 1;

    for (size_t i = 0; i + 2 < vertex_count; i += 3) {
        const float x0 = screen_x_[i];
        const float y0 = screen_y_[i];
        const float x1 = screen_x_[i + 1];
        const float y1 = screen_y_[i + 1];
        const float x2 = screen_x_[i + 2];
        const float y2 = screen_y_[i + 2];

        // Cheap reject when the whole triangle is outside one screen edge.
        if ((x0 < min_x && x1 < min_x && x2 < min_x) ||
            (x0 > max_x && x1 > max_x && x2 > max_x) ||
            (y0 < min_y && y1 < min_y && y2 < min_y) ||
            (y0 > max_y && y1 > max_y && y2 > max_y)) {
            continue;
        }

        float avg_depth = (depth_[i] + depth_[i + 1] + depth_[i + 2]) / 3.0f;
        float t = (avg_depth + 1.0f) * 0.5f;
        uint16_t color = lerp565(Theme::kWireFar, Theme::kWireNear, t);

        const int ix0 = static_cast<int>(std::lround(x0));
        const int iy0 = static_cast<int>(std::lround(y0));
        const int ix1 = static_cast<int>(std::lround(x1));
        const int iy1 = static_cast<int>(std::lround(y1));
        const int ix2 = static_cast<int>(std::lround(x2));
        const int iy2 = static_cast<int>(std::lround(y2));

        canvas.line(ix0, iy0, ix1, iy1, color);
        canvas.line(ix1, iy1, ix2, iy2, color);
        canvas.line(ix2, iy2, ix0, iy0, color);
    }
}
