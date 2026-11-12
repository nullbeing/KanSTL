#include "renderer.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kAmbient = 0.30f;
constexpr float kDiffuse = 0.70f;

// Light direction in camera space, from the upper left front.
constexpr float kLightX = -0.352f;
constexpr float kLightY = 0.553f;
constexpr float kLightZ = 0.755f;

constexpr uint16_t kBaseColor = rgb565(120, 200, 230);

inline float edge(float ax, float ay, float bx, float by, float px,
                  float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

inline uint16_t shade(uint16_t base, float intensity) {
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    int r = (base >> 11) & 0x1F;
    int g = (base >> 5) & 0x3F;
    int b = base & 0x1F;
    r = static_cast<int>(r * intensity + 0.5f);
    g = static_cast<int>(g * intensity + 0.5f);
    b = static_cast<int>(b * intensity + 0.5f);
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

}  // namespace

Renderer::Renderer() { buildMatcap(); }

void Renderer::buildMatcap() {
    constexpr float light_x = -0.40f;
    constexpr float light_y = 0.50f;
    constexpr float light_z = 0.76f;
    constexpr float half_x = -0.213f;
    constexpr float half_y = 0.267f;
    constexpr float half_z = 0.940f;

    for (int y = 0; y < 32; ++y) {
        const float ny = (y + 0.5f) / 32.0f * 2.0f - 1.0f;
        for (int x = 0; x < 32; ++x) {
            const float nx = (x + 0.5f) / 32.0f * 2.0f - 1.0f;
            const float radius2 = nx * nx + ny * ny;

            uint16_t color = 0;
            if (radius2 < 1.0f) {
                const float nz = std::sqrt(1.0f - radius2);
                const float diffuse =
                    std::max(0.0f, nx * light_x + ny * light_y +
                                       nz * light_z);
                const float specular = std::pow(
                    std::max(0.0f, nx * half_x + ny * half_y + nz * half_z),
                    26.0f);
                const float rim = std::pow(1.0f - nz, 3.0f);

                const float r = 18.0f + 100.0f * diffuse +
                                160.0f * specular + 25.0f * rim;
                const float g = 45.0f + 145.0f * diffuse +
                                160.0f * specular + 35.0f * rim;
                const float b = 65.0f + 150.0f * diffuse +
                                155.0f * specular + 45.0f * rim;
                const auto to_byte = [](float value) {
                    return static_cast<uint8_t>(
                        std::max(0.0f, std::min(255.0f, value)));
                };
                color = rgb565(to_byte(r), to_byte(g), to_byte(b));
            }
            matcap_[y * 32 + x] = color;
        }
    }
}

uint16_t Renderer::matcapColor(float nx, float ny) const {
    const int x = std::max(
        0, std::min(31, static_cast<int>((nx + 1.0f) * 15.5f)));
    const int y = std::max(
        0, std::min(31, static_cast<int>((ny + 1.0f) * 15.5f)));
    return matcap_[y * 32 + x];
}

void Renderer::project(const Mesh& mesh, const Camera& camera,
                       const Rect& viewport) {
    const size_t vertex_count = mesh.vertices.size();
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
        const float vx = v.x - camera.pivot_x;
        const float vy = v.y - camera.pivot_y;
        const float vz = v.z - camera.pivot_z;

        // Pitch around X, then yaw around Y.
        const float y1 = vy * cp - vz * sp;
        const float z1 = vy * sp + vz * cp;
        const float x2 = vx * cy + z1 * sy;
        const float z2 = -vx * sy + z1 * cy;

        screen_x_[i] = center_x + x2 * scale;
        screen_y_[i] = center_y - y1 * scale;
        depth_[i] = z2;
    }
}

void Renderer::renderWireframe(Canvas& canvas, const Rect& viewport) {
    const int min_x = viewport.x;
    const int min_y = viewport.y;
    const int max_x = viewport.x + viewport.w - 1;
    const int max_y = viewport.y + viewport.h - 1;

    for (size_t i = 0; i + 2 < screen_x_.size(); i += 3) {
        const float x0 = screen_x_[i];
        const float y0 = screen_y_[i];
        const float x1 = screen_x_[i + 1];
        const float y1 = screen_y_[i + 1];
        const float x2 = screen_x_[i + 2];
        const float y2 = screen_y_[i + 2];

        if ((x0 < min_x && x1 < min_x && x2 < min_x) ||
            (x0 > max_x && x1 > max_x && x2 > max_x) ||
            (y0 < min_y && y1 < min_y && y2 < min_y) ||
            (y0 > max_y && y1 > max_y && y2 > max_y)) {
            continue;
        }

        const float avg_depth =
            (depth_[i] + depth_[i + 1] + depth_[i + 2]) / 3.0f;
        const float t = (avg_depth + 1.0f) * 0.5f;
        const uint16_t color =
            lerp565(Theme::kWireFar, Theme::kWireNear, t);

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

void Renderer::renderSolid(const Mesh& mesh, Canvas& canvas,
                           const Rect& viewport, float cp, float sp,
                           float cy, float sy, bool matcap) {
    const size_t pixel_count =
        static_cast<size_t>(canvas.width()) * canvas.height();
    zbuffer_.resize(pixel_count);
    std::fill(zbuffer_.begin(), zbuffer_.end(), -1e30f);

    uint16_t* pixels = canvas.data();
    const int canvas_width = canvas.width();
    const int min_x = viewport.x;
    const int min_y = viewport.y;
    const int max_x = viewport.x + viewport.w - 1;
    const int max_y = viewport.y + viewport.h - 1;

    for (size_t i = 0; i + 2 < screen_x_.size(); i += 3) {
        const float x0 = screen_x_[i];
        const float y0 = screen_y_[i];
        const float x1 = screen_x_[i + 1];
        const float y1 = screen_y_[i + 1];
        const float x2 = screen_x_[i + 2];
        const float y2 = screen_y_[i + 2];

        // Screen y points down, so front-facing triangles have positive area.
        const float area = edge(x0, y0, x1, y1, x2, y2);
        if (area <= 0.0001f) continue;

        if ((x0 < min_x && x1 < min_x && x2 < min_x) ||
            (x0 > max_x && x1 > max_x && x2 > max_x) ||
            (y0 < min_y && y1 < min_y && y2 < min_y) ||
            (y0 > max_y && y1 > max_y && y2 > max_y)) {
            continue;
        }

        const int bx0 = std::max(
            min_x, static_cast<int>(std::floor(std::min({x0, x1, x2}))));
        const int bx1 = std::min(
            max_x, static_cast<int>(std::ceil(std::max({x0, x1, x2}))));
        const int by0 = std::max(
            min_y, static_cast<int>(std::floor(std::min({y0, y1, y2}))));
        const int by1 = std::min(
            max_y, static_cast<int>(std::ceil(std::max({y0, y1, y2}))));
        if (bx1 < bx0 || by1 < by0) continue;

        const Vec3& normal = mesh.normals[i / 3];
        const float ny = normal.y * cp - normal.z * sp;
        const float nz1 = normal.y * sp + normal.z * cp;
        const float nx = normal.x * cy + nz1 * sy;
        const float nz = -normal.x * sy + nz1 * cy;

        uint16_t color = 0;
        if (matcap) {
            color = matcapColor(nx, ny);
        } else {
            const float diffuse =
                std::fabs(nx * kLightX + ny * kLightY + nz * kLightZ);
            const float intensity = kAmbient + kDiffuse * diffuse;
            color = shade(kBaseColor, intensity);
        }

        const float inv_area = 1.0f / area;
        for (int y = by0; y <= by1; ++y) {
            const float py = static_cast<float>(y) + 0.5f;
            for (int x = bx0; x <= bx1; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float w0 = edge(x1, y1, x2, y2, px, py);
                if (w0 < 0.0f) continue;
                const float w1 = edge(x2, y2, x0, y0, px, py);
                if (w1 < 0.0f) continue;
                const float w2 = edge(x0, y0, x1, y1, px, py);
                if (w2 < 0.0f) continue;

                const float z = (w0 * depth_[i] + w1 * depth_[i + 1] +
                                 w2 * depth_[i + 2]) *
                                inv_area;
                const size_t index =
                    static_cast<size_t>(y) * canvas_width + x;
                if (z <= zbuffer_[index]) continue;
                zbuffer_[index] = z;
                pixels[index] = color;
            }
        }
    }
}

void Renderer::renderFeatureEdges(const Mesh& mesh, const Camera& camera,
                                  Canvas& canvas, const Rect& viewport,
                                  float cp, float sp, float cy, float sy) {
    const auto& vertices = mesh.welded_vertices;
    if (vertices.empty() || mesh.edges.empty() || zbuffer_.empty()) return;

    const float base_scale =
        0.5f * static_cast<float>(std::min(viewport.w, viewport.h)) * 0.92f;
    const float scale = base_scale * camera.zoom;
    const float center_x = viewport.x + viewport.w * 0.5f;
    const float center_y = viewport.y + viewport.h * 0.5f;

    edge_screen_x_.resize(vertices.size());
    edge_screen_y_.resize(vertices.size());
    edge_depth_.resize(vertices.size());

    for (size_t i = 0; i < vertices.size(); ++i) {
        const Vec3& v = vertices[i];
        const float vx = v.x - camera.pivot_x;
        const float vy = v.y - camera.pivot_y;
        const float vz = v.z - camera.pivot_z;

        const float y1 = vy * cp - vz * sp;
        const float z1 = vy * sp + vz * cp;
        const float x2 = vx * cy + z1 * sy;
        const float z2 = -vx * sy + z1 * cy;

        edge_screen_x_[i] = center_x + x2 * scale;
        edge_screen_y_[i] = center_y - y1 * scale;
        edge_depth_[i] = z2;
    }

    // Camera-space normal Z, used to detect silhouette edges.
    std::vector<float> normal_z(mesh.triangleCount());
    for (size_t i = 0; i < mesh.triangleCount(); ++i) {
        const Vec3& n = mesh.normals[i];
        const float nz1 = n.y * sp + n.z * cp;
        normal_z[i] = -n.x * sy + nz1 * cy;
    }

    const int min_x = viewport.x;
    const int min_y = viewport.y;
    const int max_x = viewport.x + viewport.w - 1;
    const int max_y = viewport.y + viewport.h - 1;
    const int canvas_width = canvas.width();
    constexpr uint16_t kCreaseColor = 0x0210;      // dark navy
    constexpr uint16_t kSilhouetteColor = 0x9EFF;  // light cyan
    constexpr float kDepthBias = 0.02f;

    for (const MeshEdge& edge : mesh.edges) {
        if (edge.a >= vertices.size() || edge.b >= vertices.size()) continue;

        bool silhouette = edge.f1 == 0xFFFFFFFFu;
        if (!silhouette) {
            const bool front0 = normal_z[edge.f0] > 0.0f;
            const bool front1 = normal_z[edge.f1] > 0.0f;
            silhouette = front0 != front1;
        }
        if (!edge.feature && !silhouette) continue;

        const uint16_t edge_color =
            silhouette ? kSilhouetteColor : kCreaseColor;

        const float x0 = edge_screen_x_[edge.a];
        const float y0 = edge_screen_y_[edge.a];
        const float z0 = edge_depth_[edge.a];
        const float x1 = edge_screen_x_[edge.b];
        const float y1 = edge_screen_y_[edge.b];
        const float z1 = edge_depth_[edge.b];
        const float dx = x1 - x0;
        const float dy = y1 - y0;

        float t0 = 0.0f;
        float t1 = 1.0f;
        auto clip = [&](float p, float q) {
            if (p == 0.0f) return q >= 0.0f;
            const float r = q / p;
            if (p < 0.0f) {
                if (r > t1) return false;
                if (r > t0) t0 = r;
            } else {
                if (r < t0) return false;
                if (r < t1) t1 = r;
            }
            return true;
        };

        if (!clip(-dx, x0 - min_x) ||
            !clip(dx, max_x - x0) ||
            !clip(-dy, y0 - min_y) ||
            !clip(dy, max_y - y0)) {
            continue;
        }

        const float cx0 = x0 + t0 * dx;
        const float cy0 = y0 + t0 * dy;
        const float cx1 = x0 + t1 * dx;
        const float cy1 = y0 + t1 * dy;
        const float cz0 = z0 + t0 * (z1 - z0);
        const float cz1 = z0 + t1 * (z1 - z0);

        const int ix0 = static_cast<int>(std::lround(cx0));
        const int iy0 = static_cast<int>(std::lround(cy0));
        const int ix1 = static_cast<int>(std::lround(cx1));
        const int iy1 = static_cast<int>(std::lround(cy1));
        const int steps =
            std::max(std::abs(ix1 - ix0), std::abs(iy1 - iy0));
        if (steps == 0) {
            const size_t index =
                static_cast<size_t>(iy0) * canvas_width + ix0;
            if (cz0 + kDepthBias >= zbuffer_[index]) {
                canvas.pixel(ix0, iy0, edge_color);
            }
            continue;
        }

        for (int s = 0; s <= steps; ++s) {
            const float t = static_cast<float>(s) / steps;
            const int px = static_cast<int>(
                std::lround(ix0 + (ix1 - ix0) * t));
            const int py = static_cast<int>(
                std::lround(iy0 + (iy1 - iy0) * t));
            if (px < min_x || px > max_x || py < min_y || py > max_y) {
                continue;
            }
            const float z = cz0 + (cz1 - cz0) * t;
            const size_t index =
                static_cast<size_t>(py) * canvas_width + px;
            if (z + kDepthBias >= zbuffer_[index]) {
                canvas.pixel(px, py, edge_color);
            }
        }
    }
}

void Renderer::render(const Mesh& mesh, const Camera& camera,
                      Canvas& canvas, const Rect& viewport,
                      RenderMode mode) {
    if (mesh.vertices.empty() || viewport.w <= 0 || viewport.h <= 0) return;

    project(mesh, camera, viewport);

    if (mode == RenderMode::Wireframe) {
        renderWireframe(canvas, viewport);
        return;
    }
    if (mesh.normals.size() < mesh.triangleCount()) return;

    const float yaw = camera.yaw_deg * kDegToRad;
    const float pitch = camera.pitch_deg * kDegToRad;
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);

    renderSolid(mesh, canvas, viewport, cp, sp, cy, sy,
                mode == RenderMode::Matcap);
    if (mode == RenderMode::Edges) {
        renderFeatureEdges(mesh, camera, canvas, viewport, cp, sp, cy, sy);
    }
}
