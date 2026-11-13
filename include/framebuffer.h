#ifndef KANSTL_FRAMEBUFFER_H
#define KANSTL_FRAMEBUFFER_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "canvas.h"

// Minimal Linux framebuffer output for the Cardputer Zero panel.
class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    bool open(const std::string& device, std::string& error);
    void close();

    bool isOpen() const { return fd_ >= 0; }
    int width() const { return width_; }
    int height() const { return height_; }

    void present(const Canvas& canvas);

    // Blit a sprite straight to the panel, skipping pixels equal to
    // transparent_key. Used for transient overlays that must not appear in
    // screenshots saved from the canvas.
    void overlay(const Canvas& sprite, int x, int y, uint16_t transparent_key);

    void keepDisplayOn();

private:
    int fd_ = -1;
    uint8_t* memory_ = nullptr;
    size_t memory_size_ = 0;
    size_t stride_ = 0;
    int width_ = 0;
    int height_ = 0;
    int bits_per_pixel_ = 0;
};

#endif  // KANSTL_FRAMEBUFFER_H
