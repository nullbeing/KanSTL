#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "canvas.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "png.h"
#include "renderer.h"
#include "stl_loader.h"
#include "ui.h"

namespace {

constexpr int kCanvasWidth = 320;
constexpr int kCanvasHeight = 170;

volatile sig_atomic_t g_running = 1;

void onSignal(int) { g_running = 0; }

struct Options {
    std::string file;
    std::string fb_device;
    std::string kbd_device;
    std::string shot_path;
    float yaw = -30.0f;
    float pitch = 20.0f;
    float zoom = 1.0f;
    int bench_frames = 0;
    bool no_ui = false;
    bool auto_start = false;
    bool wireframe = false;
};

void printUsage(const char* argv0) {
    std::printf(
        "stlviewer - STL viewer for the M5Stack Cardputer Zero\n"
        "\n"
        "Usage: %s [options] [model.stl]\n"
        "\n"
        "Options:\n"
        "  --shot <file.png>   Render one frame to PNG and exit\n"
        "  --yaw <deg>         Initial yaw (default -30)\n"
        "  --pitch <deg>       Initial pitch (default 20)\n"
        "  --zoom <factor>     Initial zoom (default 1.0)\n"
        "  --bench [frames]    Render frames offscreen and report speed\n"
        "  --fb <device>       Framebuffer device (default /dev/fb0)\n"
        "  --kbd <device>      Keyboard event device (default autodetect)\n"
        "  --auto              Start with auto-rotate enabled\n"
        "  --wireframe         Start in wireframe mode (default: solid)\n"
        "  --no-ui             Hide status and hint bars\n"
        "  -h, --help          Show this help\n"
        "\n"
        "Interactive keys:\n"
        "  Arrows rotate, +/- zoom, R reset, Enter fit,\n"
        "  A auto-rotate, S screenshot, H help, Esc (hold) exit.\n",
        argv0);
}

bool parseInt(const char* value, int& out) {
    if (value == nullptr || *value == '\0') return false;
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > 100000) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

bool parseFloat(const char* value, float& out) {
    if (value == nullptr || *value == '\0') return false;
    char* end = nullptr;
    out = std::strtof(value, &end);
    return end != value && *end == '\0' && std::isfinite(out);
}

bool parseArgs(int argc, char** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--shot" && i + 1 < argc) {
            options.shot_path = argv[++i];
        } else if (arg == "--yaw" && i + 1 < argc) {
            if (!parseFloat(argv[++i], options.yaw)) {
                error = "invalid --yaw value";
                return false;
            }
        } else if (arg == "--pitch" && i + 1 < argc) {
            if (!parseFloat(argv[++i], options.pitch)) {
                error = "invalid --pitch value";
                return false;
            }
        } else if (arg == "--zoom" && i + 1 < argc) {
            if (!parseFloat(argv[++i], options.zoom)) {
                error = "invalid --zoom value";
                return false;
            }
        } else if (arg == "--bench") {
            options.bench_frames = 200;
            int frames = 0;
            if (i + 1 < argc && parseInt(argv[i + 1], frames)) {
                options.bench_frames = frames;
                ++i;
            }
        } else if (arg == "--fb" && i + 1 < argc) {
            options.fb_device = argv[++i];
        } else if (arg == "--kbd" && i + 1 < argc) {
            options.kbd_device = argv[++i];
        } else if (arg == "--auto") {
            options.auto_start = true;
        } else if (arg == "--wireframe") {
            options.wireframe = true;
        } else if (arg == "--no-ui") {
            options.no_ui = true;
        } else if (!arg.empty() && arg[0] == '-') {
            error = "unknown option: " + arg;
            return false;
        } else if (options.file.empty()) {
            options.file = arg;
        } else {
            error = "only one model file can be opened";
            return false;
        }
    }
    return true;
}

std::string scanForStl(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return {};

    // Pick the most recently modified model so a freshly uploaded file opens
    // without having to rename or delete older ones.
    std::string newest;
    std::filesystem::file_time_type newest_time{};
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (ext != ".stl") continue;

        std::error_code time_error;
        const auto modified = entry.last_write_time(time_error);
        if (time_error) continue;
        if (newest.empty() || modified > newest_time) {
            newest = entry.path().string();
            newest_time = modified;
        }
    }
    return newest;
}

std::string findDefaultModel() {
    if (const char* env = std::getenv("STLVIEWER_MODEL")) {
        if (*env) return env;
    }
    if (std::string found = scanForStl(std::filesystem::current_path());
        !found.empty()) {
        return found;
    }
    if (const char* home = std::getenv("HOME")) {
        if (std::string found = scanForStl(std::filesystem::path(home) / "models");
            !found.empty()) {
            return found;
        }
        if (std::string found = scanForStl(home); !found.empty()) {
            return found;
        }
    }
    return {};
}

Rect activeViewport(const Canvas& canvas, bool no_ui) {
    if (no_ui) return Rect{0, 0, canvas.width(), canvas.height()};
    return viewportRect(canvas.width(), canvas.height());
}

void renderFrame(Canvas& canvas, const Mesh& mesh, const Camera& camera,
                 Renderer& renderer, const UiState& ui, bool no_ui,
                 RenderMode mode) {
    canvas.clear(Theme::kBackground);
    renderer.render(mesh, camera, canvas, activeViewport(canvas, no_ui), mode);
    if (!no_ui) drawUi(canvas, ui);
}

void applyRotationKey(Key key, Camera& camera) {
    switch (key) {
        case Key::Up:
            camera.pitch_deg += 5.0f;
            break;
        case Key::Down:
            camera.pitch_deg -= 5.0f;
            break;
        case Key::Left:
            camera.yaw_deg -= 5.0f;
            break;
        case Key::Right:
            camera.yaw_deg += 5.0f;
            break;
        case Key::Plus:
            camera.zoom *= 1.10f;
            break;
        case Key::Minus:
            camera.zoom /= 1.10f;
            break;
        default:
            return;
    }

    camera.pitch_deg = std::max(-89.0f, std::min(89.0f, camera.pitch_deg));
    while (camera.yaw_deg < -180.0f) camera.yaw_deg += 360.0f;
    while (camera.yaw_deg > 180.0f) camera.yaw_deg -= 360.0f;
    camera.zoom = std::max(0.15f, std::min(12.0f, camera.zoom));
}

bool isRepeatable(Key key) {
    return key == Key::Up || key == Key::Down || key == Key::Left ||
           key == Key::Right || key == Key::Plus || key == Key::Minus;
}

size_t keyIndex(Key key) { return static_cast<size_t>(key); }

int runInteractive(const Options& options, const Mesh& mesh) {
    Framebuffer framebuffer;
    std::string error;
    if (!framebuffer.open(options.fb_device, error)) {
        std::fprintf(stderr, "stlviewer: %s\n", error.c_str());
        return 1;
    }
    framebuffer.keepDisplayOn();

    Keyboard keyboard;
    if (!keyboard.open(options.kbd_device, error)) {
        std::fprintf(stderr, "stlviewer: %s\n", error.c_str());
        return 1;
    }

    Canvas canvas(framebuffer.width(), framebuffer.height());
    Renderer renderer;

    Camera camera{options.yaw, options.pitch, options.zoom};
    const Camera home_camera = camera;

    bool running = true;
    bool dirty = true;
    bool help = false;
    bool auto_rotate = options.auto_start;
    bool wireframe = options.wireframe;
    int fps = 0;
    std::string toast;
    auto toast_until = std::chrono::steady_clock::now();

    struct HeldKey {
        bool active = false;
        std::chrono::steady_clock::time_point next_repeat;
    };
    std::array<HeldKey, 16> held;

    auto now = std::chrono::steady_clock::now();
    auto last_frame = now;
    auto last_present = now;
    auto last_keepalive = now;
    auto esc_pressed = now;
    double last_render_seconds = 0.0;

    auto saveScreenshot = [&]() {
        const std::string path = "/tmp/stlviewer_shot.png";
        if (saveCanvasPng(canvas, path)) {
            toast = "SAVED " + path;
        } else {
            toast = "SCREENSHOT FAILED";
        }
        toast_until = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(1600);
        dirty = true;
    };

    while (running && g_running) {
        // Pace continuous rendering at roughly 60 fps, but keep polling
        // often enough for responsive key handling.
        auto loop_now = std::chrono::steady_clock::now();
        int timeout_ms = 16;
        if (dirty || auto_rotate) {
            const double since_present =
                std::chrono::duration<double>(loop_now - last_present).count() *
                1000.0;
            // Rendering happens after poll, so reserve the previous frame's
            // render time from the 16 ms budget as well.
            const double wait =
                16.0 - since_present - last_render_seconds * 1000.0;
            timeout_ms = wait <= 0.0 ? 0 : static_cast<int>(wait);
        }

        struct pollfd pfd {};
        pfd.fd = keyboard.fd();
        pfd.events = POLLIN;
        int poll_result = poll(&pfd, 1, timeout_ms);
        if (poll_result > 0) {
            Key key;
            bool pressed;
            while (keyboard.readEvent(key, pressed)) {
                if (pressed) {
                    if (key == Key::Reset) {
                        camera = home_camera;
                        dirty = true;
                    } else if (key == Key::Fit) {
                        camera.zoom = 1.0f;
                        dirty = true;
                    } else if (key == Key::Auto) {
                        auto_rotate = !auto_rotate;
                        dirty = true;
                    } else if (key == Key::Wire) {
                        wireframe = !wireframe;
                        dirty = true;
                    } else if (key == Key::Help) {
                        help = !help;
                        dirty = true;
                    } else if (key == Key::Shot) {
                        saveScreenshot();
                    } else if (key == Key::Esc) {
                        esc_pressed = std::chrono::steady_clock::now();
                    } else if (isRepeatable(key)) {
                        applyRotationKey(key, camera);
                        HeldKey& state = held[keyIndex(key)];
                        state.active = true;
                        state.next_repeat =
                            std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(300);
                        dirty = true;
                    }
                } else {
                    if (key == Key::Esc) {
                        const auto duration =
                            std::chrono::steady_clock::now() - esc_pressed;
                        if (duration >= std::chrono::milliseconds(600)) {
                            running = false;
                        } else if (help) {
                            help = false;
                            dirty = true;
                        }
                    } else if (isRepeatable(key)) {
                        held[keyIndex(key)].active = false;
                    }
                }
            }
        }

        now = std::chrono::steady_clock::now();
        const double dt =
            std::chrono::duration<double>(now - last_frame).count();
        last_frame = now;

        for (size_t i = 0; i < held.size(); ++i) {
            if (!held[i].active) continue;
            if (now < held[i].next_repeat) continue;
            applyRotationKey(static_cast<Key>(i), camera);
            held[i].next_repeat = now + std::chrono::milliseconds(20);
            dirty = true;
        }

        if (auto_rotate) {
            camera.yaw_deg += 40.0f * static_cast<float>(std::min(dt, 0.1));
            while (camera.yaw_deg > 180.0f) camera.yaw_deg -= 360.0f;
            dirty = true;
        }

        if (!toast.empty() && now >= toast_until) {
            toast.clear();
            dirty = true;
        }

        if (dirty) {
            UiState ui;
            ui.mesh = &mesh;
            ui.camera = camera;
            ui.fps = fps;
            ui.auto_rotate = auto_rotate;
            ui.wireframe = wireframe;
            ui.help = help;
            ui.toast = toast;

            const auto render_start = std::chrono::steady_clock::now();
            renderFrame(canvas, mesh, camera, renderer, ui, options.no_ui,
                        wireframe ? RenderMode::Wireframe
                                  : RenderMode::Solid);
            framebuffer.present(canvas);
            const auto render_end = std::chrono::steady_clock::now();
            const double render_seconds =
                std::chrono::duration<double>(render_end - render_start)
                    .count();
            last_render_seconds =
                last_render_seconds <= 0.0
                    ? render_seconds
                    : last_render_seconds * 0.7 + render_seconds * 0.3;

            // Report the rate frames actually reach the panel. Idle gaps are
            // ignored so a paused viewer keeps its last active rate.
            const auto present_now = render_end;
            const double interval =
                std::chrono::duration<double>(present_now - last_present)
                    .count();
            if (interval > 0.0001 && interval < 0.25) {
                const int sample =
                    static_cast<int>(std::lround(1.0 / interval));
                fps = fps == 0 ? sample : (fps * 3 + sample) / 4;
            }
            last_present = present_now;
            dirty = false;
        }

        if (now - last_keepalive >= std::chrono::seconds(1)) {
            framebuffer.keepDisplayOn();
            last_keepalive = now;
        }
    }

    // Leave the launcher a clean screen.
    canvas.clear(Theme::kBackground);
    framebuffer.present(canvas);
    return 0;
}

int runBenchmark(const Options& options, const Mesh& mesh) {
    Canvas canvas(kCanvasWidth, kCanvasHeight);
    Renderer renderer;
    Camera camera{options.yaw, options.pitch, options.zoom};
    UiState ui;
    ui.mesh = &mesh;
    ui.camera = camera;
    ui.wireframe = options.wireframe;
    const RenderMode mode =
        options.wireframe ? RenderMode::Wireframe : RenderMode::Solid;

    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < options.bench_frames; ++frame) {
        camera.yaw_deg += 1.5f;
        if (camera.yaw_deg > 180.0f) camera.yaw_deg -= 360.0f;
        ui.camera = camera;
        renderFrame(canvas, mesh, camera, renderer, ui, options.no_ui, mode);
    }
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    const double ms = seconds * 1000.0 / options.bench_frames;
    const double fps = options.bench_frames / seconds;
    std::printf("bench: %zu triangles, %d frames, %.2f ms/frame, %.1f fps\n",
                mesh.triangleCount(), options.bench_frames, ms, fps);
    return 0;
}

int renderScreenshot(const Options& options, Mesh& mesh) {
    Canvas canvas(kCanvasWidth, kCanvasHeight);
    Renderer renderer;
    Camera camera{options.yaw, options.pitch, options.zoom};

    UiState ui;
    ui.mesh = &mesh;
    ui.camera = camera;
    ui.fps = 0;
    ui.wireframe = options.wireframe;

    renderFrame(canvas, mesh, camera, renderer, ui, options.no_ui,
                options.wireframe ? RenderMode::Wireframe : RenderMode::Solid);

    if (!saveCanvasPng(canvas, options.shot_path)) {
        std::fprintf(stderr, "stlviewer: failed to write %s\n",
                     options.shot_path.c_str());
        return 1;
    }
    std::printf("wrote %s (%dx%d, %zu triangles)\n", options.shot_path.c_str(),
                canvas.width(), canvas.height(), mesh.triangleCount());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    std::string error;
    if (!parseArgs(argc, argv, options, error)) {
        std::fprintf(stderr, "stlviewer: %s\n\n", error.c_str());
        printUsage(argv[0]);
        return 1;
    }

    if (options.file.empty()) options.file = findDefaultModel();
    if (options.file.empty()) {
        std::fprintf(stderr,
                     "stlviewer: no STL file given and none found in the "
                     "current directory or $HOME\n\n");
        printUsage(argv[0]);
        return 1;
    }

    Mesh mesh;
    if (!loadStl(options.file, mesh, error)) {
        std::fprintf(stderr, "stlviewer: %s: %s\n", options.file.c_str(),
                     error.c_str());
        return 1;
    }
    std::fprintf(stderr, "model: %s, %zu triangles\n", mesh.name.c_str(),
                 mesh.triangleCount());

    if (options.bench_frames > 0) {
        return runBenchmark(options, mesh);
    }

    if (!options.shot_path.empty()) {
        return renderScreenshot(options, mesh);
    }

    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    return runInteractive(options, mesh);
}
