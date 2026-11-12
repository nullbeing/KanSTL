#ifndef STLVIEWER_CANVAS_H
#define STLVIEWER_CANVAS_H

#include <cstdint>
#include <string>
#include <vector>

// RGB565 software canvas. All UI and 3D drawing goes through this class so
// the same renderer can feed /dev/fb0 or an offscreen PNG.
class Canvas {
public:
    Canvas(int width = 0, int height = 0);

    void resize(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }

    uint16_t* data() { return pixels_.data(); }
    const uint16_t* data() const { return pixels_.data(); }

    void clear(uint16_t color);

    void pixel(int x, int y, uint16_t color);
    void blendPixel(int x, int y, uint16_t color, float alpha);
    void line(int x0, int y0, int x1, int y1, uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void rect(int x, int y, int w, int h, uint16_t color);

    // 6x8 bitmap text.
    void text(int x, int y, const std::string& text, uint16_t color);
    int textWidth(const std::string& text) const;

private:
    int width_;
    int height_;
    std::vector<uint16_t> pixels_;
};

// RGB565 helpers.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) |
                                 ((g & 0xFC) << 3) |
                                 (b >> 3));
}

uint16_t lerp565(uint16_t a, uint16_t b, float t);

namespace Theme {
constexpr uint16_t kBackground = 0x0841;  // near-black blue
constexpr uint16_t kBarBackground = 0x1082;
constexpr uint16_t kBarBorder = 0x2124;
constexpr uint16_t kText = 0x9E9F;        // zeroSDR light blue-grey
constexpr uint16_t kTextBright = 0xC618;
constexpr uint16_t kTextDim = 0x4A69;
constexpr uint16_t kAccent = 0x4FDF;      // cyan
constexpr uint16_t kWireNear = 0x4FDF;    // bright cyan
constexpr uint16_t kWireFar = 0x1A69;     // dark teal
constexpr uint16_t kPillBackground = 0xAEDC;  // light blue
constexpr uint16_t kPillActive = 0x9725;      // mint green
constexpr uint16_t kPillText = 0x0000;        // black
constexpr uint16_t kDialogShadow = 0x42CF;      // steel-blue halo
constexpr uint16_t kDialogBackground = 0x18C3;  // raised dark slate
constexpr uint16_t kDialogBorder = 0x4A69;      // soft border
}  // namespace Theme

#endif  // STLVIEWER_CANVAS_H
