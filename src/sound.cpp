#include "sound.h"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr int kSampleRate = 48000;

struct Note {
    float frequency;
    int duration_ms;
    float volume;
};

std::vector<Note> notesFor(SoundEffect effect) {
    switch (effect) {
        case SoundEffect::Key:
            return {{1200.0f, 16, 0.08f}};
        case SoundEffect::Confirm:
            return {{880.0f, 30, 0.10f}, {1320.0f, 50, 0.10f}};
        case SoundEffect::Back:
            return {{520.0f, 40, 0.08f}};
        case SoundEffect::ToggleOn:
            return {{700.0f, 30, 0.09f}, {1000.0f, 40, 0.09f}};
        case SoundEffect::ToggleOff:
            return {{1000.0f, 30, 0.09f}, {700.0f, 40, 0.09f}};
        case SoundEffect::Error:
            return {{220.0f, 100, 0.14f}, {165.0f, 120, 0.14f}};
        case SoundEffect::Success:
            return {{880.0f, 40, 0.10f}, {1320.0f, 40, 0.10f},
                    {1760.0f, 60, 0.10f}};
    }
    return {};
}

std::vector<int16_t> synthesize(SoundEffect effect) {
    constexpr float kTwoPi = 6.283185307179586f;
    const int fade = kSampleRate * 2 / 1000;  // 2 ms
    std::vector<int16_t> samples;

    for (const Note& note : notesFor(effect)) {
        const int count = kSampleRate * note.duration_ms / 1000;
        for (int i = 0; i < count; ++i) {
            float envelope = 1.0f;
            if (i < fade) {
                envelope = static_cast<float>(i) / fade;
            } else if (i > count - fade) {
                envelope = static_cast<float>(count - i) / fade;
            }
            const float phase =
                kTwoPi * note.frequency * static_cast<float>(i) / kSampleRate;
            const float value =
                std::sin(phase) * note.volume * std::max(0.0f, envelope);
            samples.push_back(static_cast<int16_t>(value * 32767.0f));
        }
    }
    return samples;
}

}  // namespace

struct SoundPlayer::Impl {
    snd_pcm_t* pcm = nullptr;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<SoundEffect> queue;
    bool running = true;
    std::atomic<bool> enabled{true};

    void openPcm() {
        static const char* candidates[] = {
            "default", "pulse", "pipewire", "plughw:1,0", "plughw:0,0"};

        for (const char* name : candidates) {
            snd_pcm_t* handle = nullptr;
            if (snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK,
                             SND_PCM_NONBLOCK) < 0) {
                continue;
            }
            if (snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE,
                                   SND_PCM_ACCESS_RW_INTERLEAVED, 1,
                                   kSampleRate, 1, 30000) < 0) {
                snd_pcm_close(handle);
                continue;
            }
            snd_pcm_nonblock(handle, 0);
            pcm = handle;
            return;
        }
    }

    void writeEffect(SoundEffect effect) {
        if (pcm == nullptr) return;
        const std::vector<int16_t> samples = synthesize(effect);
        size_t offset = 0;
        while (offset < samples.size()) {
            const snd_pcm_sframes_t written = snd_pcm_writei(
                pcm, samples.data() + offset, samples.size() - offset);
            if (written > 0) {
                offset += static_cast<size_t>(written);
                continue;
            }
            if (written == -EPIPE) {
                snd_pcm_prepare(pcm);
                continue;
            }
            break;
        }
    }

    void run() {
        openPcm();
        while (true) {
            SoundEffect effect;
            {
                std::unique_lock<std::mutex> lock(mutex);
                condition.wait(lock, [&] {
                    return !running || !queue.empty();
                });
                if (!running && queue.empty()) break;
                effect = queue.front();
                queue.pop_front();
            }
            writeEffect(effect);
        }
        if (pcm != nullptr) {
            snd_pcm_drop(pcm);
            snd_pcm_close(pcm);
            pcm = nullptr;
        }
    }
};

SoundPlayer::SoundPlayer() : impl_(std::make_unique<Impl>()) {
    impl_->worker = std::thread([this] { impl_->run(); });
}

SoundPlayer::~SoundPlayer() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->running = false;
    }
    impl_->condition.notify_all();
    if (impl_->worker.joinable()) impl_->worker.join();
}

void SoundPlayer::play(SoundEffect effect) {
    if (!impl_->enabled) return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->queue.size() >= 4) impl_->queue.pop_front();
        impl_->queue.push_back(effect);
    }
    impl_->condition.notify_one();
}

void SoundPlayer::toggle() {
    impl_->enabled = !impl_->enabled;
}

bool SoundPlayer::enabled() const { return impl_->enabled; }
