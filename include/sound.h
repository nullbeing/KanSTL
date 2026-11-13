#ifndef KANSTL_SOUND_H
#define KANSTL_SOUND_H

#include <memory>

enum class SoundEffect {
    Key,
    Confirm,
    Back,
    ToggleOn,
    ToggleOff,
    Error,
    Success,
};

class SoundPlayer {
public:
    SoundPlayer();
    ~SoundPlayer();

    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;

    void play(SoundEffect effect);
    void toggle();
    bool enabled() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif  // KANSTL_SOUND_H
