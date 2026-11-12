#ifndef STLVIEWER_KEYBOARD_H
#define STLVIEWER_KEYBOARD_H

#include <string>

enum class Key {
    None = 0,
    Up,
    Down,
    Left,
    Right,
    PageUp,
    PageDown,
    Enter,
    Space,
    Reset,
    Auto,
    Fit,
    Wire,
    Help,
    Shot,
    Esc,
};

class Keyboard {
public:
    Keyboard() = default;
    ~Keyboard();

    bool open(const std::string& device_path, std::string& error);
    void close();

    int fd() const { return fd_; }
    bool isOpen() const { return fd_ >= 0; }
    bool altPressed() const { return alt_; }

    // Returns one translated key event. Repeated kernel events are ignored;
    // the app implements its own hold/repeat timing.
    bool readEvent(Key& key, bool& pressed);

private:
    int fd_ = -1;
    bool alt_ = false;
};

#endif  // STLVIEWER_KEYBOARD_H
