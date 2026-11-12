#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "font.h"

Canvas::Canvas(int width, int height) : width_(width), height_(height) {
    if (width_ > 0 && height_ > 0) {
        pixels_.assign(static_cast<size_t>(width_) * height_, 0);
    }
}

void Canvas::resize(int width, int height) {
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<size_t>(width_) * height_, 0);
}

void Canvas::clear(uint16_t color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Canvas::pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    pixels_[static_cast<size_t>(y) * width_ + x] = color;
}

void Canvas::blendPixel(int x, int y, uint16_t color, float alpha) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    if (alpha <= 0.0f) return;
    if (alpha >= 1.0f) {
        pixels_[static_cast<size_t>(y) * width_ + x] = color;
        return;
    }

    const uint16_t dst = pixels_[static_cast<size_t>(y) * width_ + x];
    const int dr = (dst >> 11) & 0x1F;
    const int dg = (dst >> 5) & 0x3F;
    const int db = dst & 0x1F;
    const int sr = (color >> 11) & 0x1F;
    const int sg = (color >> 5) & 0x3F;
    const int sb = color & 0x1F;

    const int r = dr + static_cast<int>((sr - dr) * alpha + 0.5f);
    const int g = dg + static_cast<int>((sg - dg) * alpha + 0.5f);
    const int b = db + static_cast<int>((sb - db) * alpha + 0.5f);
    pixels_[static_cast<size_t>(y) * width_ + x] =
        static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

void Canvas::line(int x0, int y0, int x1, int y1, uint16_t color) {
    // Liang-Barsky clip so zoomed-in wireframes do not walk hundreds of
    // off-screen pixels per edge.
    float fx0 = static_cast<float>(x0);
    float fy0 = static_cast<float>(y0);
    float fx1 = static_cast<float>(x1);
    float fy1 = static_cast<float>(y1);
    const float dx = fx1 - fx0;
    const float dy = fy1 - fy0;
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

    if (!clip(-dx, fx0) || !clip(dx, static_cast<float>(width_ - 1) - fx0) ||
        !clip(-dy, fy0) || !clip(dy, static_cast<float>(height_ - 1) - fy0)) {
        return;
    }

    x0 = static_cast<int>(std::lround(fx0 + t0 * dx));
    y0 = static_cast<int>(std::lround(fy0 + t0 * dy));
    const int cx1 = static_cast<int>(std::lround(fx0 + t1 * dx));
    const int cy1 = static_cast<int>(std::lround(fy0 + t1 * dy));

    int ddx = std::abs(cx1 - x0);
    int ddy = -std::abs(cy1 - y0);
    int sx = x0 < cx1 ? 1 : -1;
    int sy = y0 < cy1 ? 1 : -1;
    int err = ddx + ddy;

    while (true) {
        pixel(x0, y0, color);
        if (x0 == cx1 && y0 == cy1) break;
        int e2 = 2 * err;
        if (e2 >= ddy) {
            err += ddy;
            x0 += sx;
        }
        if (e2 <= ddx) {
            err += ddx;
            y0 += sy;
        }
    }
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t color) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(width_, x + w);
    int y1 = std::min(height_, y + h);
    if (x0 >= x1 || y0 >= y1) return;

    for (int row = y0; row < y1; ++row) {
        std::fill(pixels_.begin() + static_cast<size_t>(row) * width_ + x0,
                  pixels_.begin() + static_cast<size_t>(row) * width_ + x1,
                  color);
    }
}

void Canvas::rect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    fillRect(x, y, w, 1, color);
    fillRect(x, y + h - 1, w, 1, color);
    fillRect(x, y, 1, h, color);
    fillRect(x + w - 1, y, 1, h, color);
}

void Canvas::text(int x, int y, const std::string& value, uint16_t color) {
    for (char raw : value) {
        unsigned char c = static_cast<unsigned char>(raw);
        if (c < 0x20 || c > 0x7E) c = '?';
        const uint8_t* glyph = font6x8[c - 0x20];
        for (int row = 0; row < FONT_6X8_HEIGHT; ++row) {
            uint8_t bits = glyph[row];
            if (!bits) continue;
            for (int col = 0; col < FONT_6X8_WIDTH; ++col) {
                if (bits & (0x80 >> col)) {
                    pixel(x + col, y + row, color);
                }
            }
        }
        x += FONT_6X8_WIDTH;
    }
}

int Canvas::textWidth(const std::string& value) const {
    return static_cast<int>(value.size()) * FONT_6X8_WIDTH;
}

uint16_t lerp565(uint16_t a, uint16_t b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    int ar = (a >> 11) & 0x1F;
    int ag = (a >> 5) & 0x3F;
    int ab = a & 0x1F;
    int br = (b >> 11) & 0x1F;
    int bg = (b >> 5) & 0x3F;
    int bb = b & 0x1F;

    int r = ar + static_cast<int>((br - ar) * t + 0.5f);
    int g = ag + static_cast<int>((bg - ag) * t + 0.5f);
    int bl = ab + static_cast<int>((bb - ab) * t + 0.5f);
    return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}
