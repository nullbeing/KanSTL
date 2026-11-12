#include "framebuffer.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

Framebuffer::~Framebuffer() { close(); }

void Framebuffer::close() {
    if (memory_ != nullptr) {
        munmap(memory_, memory_size_);
        memory_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    width_ = height_ = 0;
    memory_size_ = 0;
    stride_ = 0;
    bits_per_pixel_ = 0;
}

bool Framebuffer::open(const std::string& device, std::string& error) {
    close();

    std::vector<std::string> candidates;
    if (!device.empty()) {
        candidates.push_back(device);
    } else {
        candidates = {"/dev/fb0", "/dev/fb_lcd", "/dev/fb1"};
    }

    for (const std::string& path : candidates) {
        int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;

        struct fb_var_screeninfo vinfo {};
        struct fb_fix_screeninfo finfo {};
        if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
            ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
            ::close(fd);
            continue;
        }
        if (vinfo.bits_per_pixel != 16 ||
            finfo.line_length < vinfo.xres * 2u) {
            std::fprintf(stderr,
                         "fb: %s is not a usable 16 bpp RGB565 framebuffer\n",
                         path.c_str());
            ::close(fd);
            continue;
        }

        const unsigned int virtual_y =
            vinfo.yres_virtual > 0 ? vinfo.yres_virtual : vinfo.yres;
        const size_t size =
            static_cast<size_t>(finfo.line_length) * virtual_y;
        void* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, 0);
        if (memory == MAP_FAILED) {
            ::close(fd);
            continue;
        }

        fd_ = fd;
        memory_ = static_cast<uint8_t*>(memory);
        memory_size_ = size;
        stride_ = finfo.line_length;
        width_ = vinfo.xres;
        height_ = vinfo.yres;
        bits_per_pixel_ = vinfo.bits_per_pixel;
        error.clear();
        std::fprintf(stderr, "fb: %s %dx%d, %d bpp, stride %zu\n", path.c_str(),
                     width_, height_, bits_per_pixel_, stride_);
        return true;
    }

    error = device.empty() ? "no usable RGB565 framebuffer found"
                           : "cannot open framebuffer: " + device;
    return false;
}

void Framebuffer::present(const Canvas& canvas) {
    if (fd_ < 0 || canvas.width() <= 0 || canvas.height() <= 0) return;

    const int copy_width =
        canvas.width() < width_ ? canvas.width() : width_;
    const int copy_height =
        canvas.height() < height_ ? canvas.height() : height_;
    const size_t row_bytes = static_cast<size_t>(copy_width) * 2;

    for (int y = 0; y < copy_height; ++y) {
        std::memcpy(memory_ + static_cast<size_t>(y) * stride_,
                    canvas.data() + static_cast<size_t>(y) * canvas.width(),
                    row_bytes);
    }
}

void Framebuffer::keepDisplayOn() {
    if (fd_ >= 0) {
        ioctl(fd_, FBIOBLANK, FB_BLANK_UNBLANK);
    }

    const char* brightness = "/sys/class/backlight/backlight/brightness";
    int fd = ::open(brightness, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        const char value[] = "100\n";
        ssize_t ignored = ::write(fd, value, sizeof(value) - 1);
        (void)ignored;
        ::close(fd);
    }
}
