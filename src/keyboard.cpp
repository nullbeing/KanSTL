#include "keyboard.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <vector>

namespace {

bool isKeyboardDevice(int fd, std::string* name_out) {
    unsigned long ev_bits[EV_MAX / (8 * sizeof(unsigned long)) + 1] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) return false;
    if (!(ev_bits[EV_KEY / (8 * sizeof(unsigned long))] &
          (1UL << (EV_KEY % (8 * sizeof(unsigned long)))))) {
        return false;
    }

    unsigned long key_bits[KEY_MAX / (8 * sizeof(unsigned long)) + 1] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return false;
    }

    int letter_keys = 0;
    for (int k = KEY_A; k <= KEY_Z; ++k) {
        if (key_bits[k / (8 * sizeof(unsigned long))] &
            (1UL << (k % (8 * sizeof(unsigned long))))) {
            ++letter_keys;
        }
    }
    if (letter_keys < 10) return false;

    if (name_out) {
        char name[256] = {0};
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0) {
            *name_out = name;
        }
    }
    return true;
}

std::vector<std::string> findCandidates() {
    std::vector<std::string> paths;

    const char* known = "/dev/input/by-path/platform-3f804000.i2c-event";
    if (access(known, R_OK) == 0) paths.emplace_back(known);

    if (DIR* dir = opendir("/dev/input")) {
        while (dirent* entry = readdir(dir)) {
            if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
            paths.push_back(std::string("/dev/input/") + entry->d_name);
        }
        closedir(dir);
    }
    return paths;
}

Key translate(int code) {
    switch (code) {
        case KEY_UP:
        case KEY_F:
            return Key::Up;
        case KEY_DOWN:
        case KEY_X:
            return Key::Down;
        case KEY_LEFT:
        case KEY_Z:
            return Key::Left;
        case KEY_RIGHT:
        case KEY_C:
            return Key::Right;

        case KEY_R:
            return Key::Reset;
        case KEY_A:
            return Key::Auto;
        case KEY_PAGEUP:
            return Key::PageUp;
        case KEY_PAGEDOWN:
            return Key::PageDown;
        case KEY_ENTER:
        case KEY_KPENTER:
            return Key::Enter;
        case KEY_SPACE:
            return Key::Space;
        case KEY_0:
            return Key::Fit;
        case KEY_8:
            return Key::Files;
        case KEY_6:
            return Key::Auto;
        case KEY_7:
            return Key::Style;
        case KEY_H:
        case KEY_HELP:
            return Key::Help;
        case KEY_S:
            return Key::Shot;
        case KEY_B:
            return Key::Files;
        case KEY_BACKSPACE:
            return Key::Back;
        case KEY_ESC:
            return Key::Esc;

        default:
            return Key::None;
    }
}

}  // namespace

Keyboard::~Keyboard() { close(); }

void Keyboard::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    alt_ = false;
}

bool Keyboard::open(const std::string& device_path, std::string& error) {
    close();

    std::vector<std::string> candidates;
    if (!device_path.empty()) {
        candidates.push_back(device_path);
    } else {
        candidates = findCandidates();
    }

    for (const std::string& path : candidates) {
        int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        std::string name;
        if (device_path.empty() && !isKeyboardDevice(fd, &name)) {
            ::close(fd);
            continue;
        }

        fd_ = fd;
        error.clear();
        return true;
    }

    error = device_path.empty() ? "no keyboard device found"
                                : "cannot open keyboard device: " + device_path;
    return false;
}

bool Keyboard::readEvent(Key& key, bool& pressed) {
    if (fd_ < 0) return false;

    struct input_event ev;
    while (true) {
        ssize_t n = ::read(fd_, &ev, sizeof(ev));
        if (n != static_cast<ssize_t>(sizeof(ev))) {
            return false;
        }
        if (ev.type != EV_KEY) continue;

        if (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT) {
            alt_ = ev.value != 0;
            continue;
        }
        if (ev.value == 2) continue;  // kernel auto-repeat; app does its own

        Key translated = translate(ev.code);
        if (translated == Key::None) continue;

        key = translated;
        pressed = ev.value != 0;
        return true;
    }
}
