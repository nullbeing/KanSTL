#include "png.h"

#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma GCC diagnostic pop

bool saveCanvasPng(const Canvas& canvas, const std::string& path) {
    const int w = canvas.width();
    const int h = canvas.height();
    if (w <= 0 || h <= 0) return false;

    std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
    const uint16_t* src = canvas.data();
    for (int i = 0; i < w * h; ++i) {
        const uint16_t c = src[i];
        const uint8_t r = static_cast<uint8_t>((c >> 11) & 0x1F);
        const uint8_t g = static_cast<uint8_t>((c >> 5) & 0x3F);
        const uint8_t b = static_cast<uint8_t>(c & 0x1F);
        rgb[i * 3 + 0] = static_cast<uint8_t>((r << 3) | (r >> 2));
        rgb[i * 3 + 1] = static_cast<uint8_t>((g << 2) | (g >> 4));
        rgb[i * 3 + 2] = static_cast<uint8_t>((b << 3) | (b >> 2));
    }

    return stbi_write_png(path.c_str(), w, h, 3, rgb.data(), w * 3) != 0;
}
