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
#include <utility>
#include <vector>

#include "canvas.h"
#include "file_browser.h"
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

enum class AppMode {
    Browser,
    Viewer,
};

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
    RenderMode style = RenderMode::Solid;
    bool list_dir = false;
    std::string list_path;
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
        "  --list [dir]        Print the file browser contents and exit\n"
        "  --fb <device>       Framebuffer device (default /dev/fb0)\n"
        "  --kbd <device>      Keyboard event device (default autodetect)\n"
        "  --style <name>      solid (default), edges or matcap\n"
        "  --auto              Start with auto-rotate enabled\n"
        "  --wireframe         Start in wireframe mode\n"
        "  --no-ui             Hide status and hint bars\n"
        "  -h, --help          Show this help\n"
        "\n"
        "Without a model path the app opens the file browser.\n"
        "\n"
        "Viewer keys:\n"
        "  Arrows rotate, Alt+arrows move the rotation center,\n"
        "  Enter/Space zoom, R reset, 0 fit, W wire/solid,\n"
        "  B file list, A auto-rotate, S screenshot, H help,\n"
        "  hold Esc 3s to exit.\n"
        "\n"
        "Browser keys:\n"
        "  Up/Down select, PgUp/PgDn page, Enter open,\n"
        "  Left/Backspace parent, R refresh; hold Esc 3s to exit.\n",
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
        } else if (arg == "--list") {
            options.list_dir = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                options.list_path = argv[++i];
            }
        } else if (arg == "--fb" && i + 1 < argc) {
            options.fb_device = argv[++i];
        } else if (arg == "--kbd" && i + 1 < argc) {
            options.kbd_device = argv[++i];
        } else if (arg == "--auto") {
            options.auto_start = true;
        } else if (arg == "--wireframe") {
            options.wireframe = true;
        } else if (arg == "--style" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "solid") {
                options.style = RenderMode::Solid;
            } else if (value == "edges") {
                options.style = RenderMode::Edges;
            } else if (value == "matcap") {
                options.style = RenderMode::Matcap;
            } else {
                error = "invalid --style value: " + value;
                return false;
            }
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

std::string configPath() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && *xdg != '\0') {
        return std::string(xdg) + "/stlviewer/last_dir";
    }
    const char* home = std::getenv("HOME");
    return std::string(home && *home ? home : "/home/pi") +
           "/.config/stlviewer/last_dir";
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
    const Rect viewport = activeViewport(canvas, no_ui);
    canvas.clear(Theme::kBackground);
    renderer.render(mesh, camera, canvas, viewport, mode);

    const bool custom_pivot =
        std::fabs(camera.pivot_x) > 1e-4f ||
        std::fabs(camera.pivot_y) > 1e-4f ||
        std::fabs(camera.pivot_z) > 1e-4f;
    if (custom_pivot) {
        const int cx = viewport.x + viewport.w / 2;
        const int cy = viewport.y + viewport.h / 2;
        canvas.line(cx - 4, cy, cx + 4, cy, Theme::kAccent);
        canvas.line(cx, cy - 4, cx, cy + 4, Theme::kAccent);
    }

    if (!no_ui) drawUi(canvas, ui);
}

bool isArrowKey(Key key) {
    return key == Key::Up || key == Key::Down || key == Key::Left ||
           key == Key::Right;
}

// Move the rotation center along the current screen axes. The pivot is
// subtracted before rotation, so the model follows the arrow direction and
// the point under the center crosshair becomes the new orbit center.
void applyPivotKey(Key key, Camera& camera) {
    const float yaw = camera.yaw_deg * 3.14159265358979323846f / 180.0f;
    const float pitch = camera.pitch_deg * 3.14159265358979323846f / 180.0f;
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);

    // Screen right/up converted back to model space.
    const float rx = cy;
    const float ry = sy * sp;
    const float rz = sy * cp;
    const float ux = 0.0f;
    const float uy = cp;
    const float uz = -sp;
    const float step = 0.05f;

    if (key == Key::Left) {
        camera.pivot_x += rx * step;
        camera.pivot_y += ry * step;
        camera.pivot_z += rz * step;
    } else if (key == Key::Right) {
        camera.pivot_x -= rx * step;
        camera.pivot_y -= ry * step;
        camera.pivot_z -= rz * step;
    } else if (key == Key::Up) {
        camera.pivot_x -= ux * step;
        camera.pivot_y -= uy * step;
        camera.pivot_z -= uz * step;
    } else if (key == Key::Down) {
        camera.pivot_x += ux * step;
        camera.pivot_y += uy * step;
        camera.pivot_z += uz * step;
    } else {
        return;
    }

    camera.pivot_x = std::max(-1.5f, std::min(1.5f, camera.pivot_x));
    camera.pivot_y = std::max(-1.5f, std::min(1.5f, camera.pivot_y));
    camera.pivot_z = std::max(-1.5f, std::min(1.5f, camera.pivot_z));
}

void applyCameraKey(Key key, Camera& camera) {
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
        case Key::Enter:
            camera.zoom *= 1.10f;
            break;
        case Key::Space:
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

bool isViewerRepeatable(Key key) {
    return key == Key::Up || key == Key::Down || key == Key::Left ||
           key == Key::Right || key == Key::Enter || key == Key::Space;
}

bool isBrowserRepeatable(Key key) {
    return key == Key::Up || key == Key::Down || key == Key::PageUp ||
           key == Key::PageDown;
}

size_t keyIndex(Key key) { return static_cast<size_t>(key); }

int runInteractive(const Options& options) {
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

    Mesh mesh;
    bool have_model = false;
    std::string current_path;
    bool auto_rotate = options.auto_start;
    bool wireframe = options.wireframe;
    RenderMode style = options.style;

    std::string initial = options.file;
    if (initial.empty() && options.auto_start) initial = findDefaultModel();
    if (!initial.empty()) {
        std::error_code path_error;
        const std::string absolute =
            std::filesystem::absolute(initial, path_error).string();
        if (!path_error) initial = absolute;
    }

    std::string startup_error;
    if (!initial.empty()) {
        std::string load_error;
        if (loadStl(initial, mesh, load_error)) {
            have_model = true;
            current_path = initial;
            std::fprintf(stderr, "model: %s, %zu triangles\n",
                         mesh.name.c_str(), mesh.triangleCount());
        } else {
            startup_error = "LOAD FAILED: " + load_error;
        }
    }

    FileBrowser browser;
    std::string browser_error;
    std::string start_dir;
    if (!initial.empty()) {
        start_dir = std::filesystem::path(initial).parent_path().string();
    }
    browser.init(start_dir, configPath(), browser_error);
    if (!startup_error.empty()) {
        browser.setError(startup_error);
    } else if (!browser_error.empty() && browser.entries().empty()) {
        browser.setError(browser_error);
    }
    if (!current_path.empty()) browser.selectPath(current_path);

    AppMode mode = have_model ? AppMode::Viewer : AppMode::Browser;

    bool running = true;
    bool dirty = true;
    bool help = false;
    int fps = 0;
    std::string toast;
    auto toast_until = std::chrono::steady_clock::now();

    struct HeldKey {
        bool active = false;
        bool pivot = false;
        std::chrono::steady_clock::time_point next_repeat;
    };
    std::array<HeldKey, 20> held;

    auto now = std::chrono::steady_clock::now();
    auto last_frame = now;
    auto last_present = now;
    auto last_keepalive = now;
    auto esc_pressed = now;
    bool esc_down = false;
    bool exit_prompt_visible = false;
    double last_render_seconds = 0.0;
    constexpr auto kExitHold = std::chrono::milliseconds(3000);
    constexpr auto kExitPromptDelay = std::chrono::milliseconds(300);

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

    auto openModel = [&](const std::string& path) {
        drawLoading(canvas, std::filesystem::path(path).filename().string());
        framebuffer.present(canvas);

        Mesh loaded;
        std::string load_error;
        if (!loadStl(path, loaded, load_error)) {
            browser.setError("LOAD FAILED: " + load_error);
            dirty = true;
            return false;
        }

        mesh = std::move(loaded);
        have_model = true;
        current_path = path;
        camera = home_camera;
        fps = 0;
        help = false;
        toast.clear();
        mode = AppMode::Viewer;
        for (HeldKey& state : held) state.active = false;
        dirty = true;
        std::fprintf(stderr, "model: %s, %zu triangles\n", mesh.name.c_str(),
                     mesh.triangleCount());
        browser.selectPath(path);
        return true;
    };

    auto handleViewerPress = [&](Key key) {
        if (key == Key::Reset) {
            camera = home_camera;
            dirty = true;
        } else if (key == Key::Fit) {
            camera.zoom = 1.0f;
            camera.pivot_x = 0.0f;
            camera.pivot_y = 0.0f;
            camera.pivot_z = 0.0f;
            dirty = true;
        } else if (key == Key::Auto) {
            auto_rotate = !auto_rotate;
            dirty = true;
        } else if (key == Key::Style) {
            wireframe = false;
            if (style == RenderMode::Solid) {
                style = RenderMode::Edges;
            } else if (style == RenderMode::Edges) {
                style = RenderMode::Matcap;
            } else {
                style = RenderMode::Solid;
            }
            dirty = true;
        } else if (key == Key::Wire) {
            wireframe = !wireframe;
            dirty = true;
        } else if (key == Key::Files) {
            help = false;
            mode = AppMode::Browser;
            browser.reload();
            if (!current_path.empty()) {
                browser.selectPath(current_path);
            }
            for (HeldKey& state : held) state.active = false;
            dirty = true;
        } else if (key == Key::Help) {
            help = !help;
            dirty = true;
        } else if (key == Key::Shot) {
            saveScreenshot();
        } else if (isViewerRepeatable(key)) {
            HeldKey& state = held[keyIndex(key)];
            state.pivot = isArrowKey(key) && keyboard.altPressed();
            if (state.pivot) {
                applyPivotKey(key, camera);
            } else {
                applyCameraKey(key, camera);
            }
            state.active = true;
            state.next_repeat = std::chrono::steady_clock::now() +
                                std::chrono::milliseconds(300);
            dirty = true;
        }
    };

    auto handleBrowserPress = [&](Key key) {
        if (key == Key::Up) {
            browser.moveSelection(-1);
            held[keyIndex(key)].active = true;
            held[keyIndex(key)].next_repeat =
                std::chrono::steady_clock::now() +
                std::chrono::milliseconds(350);
            dirty = true;
        } else if (key == Key::Down) {
            browser.moveSelection(1);
            held[keyIndex(key)].active = true;
            held[keyIndex(key)].next_repeat =
                std::chrono::steady_clock::now() +
                std::chrono::milliseconds(350);
            dirty = true;
        } else if (key == Key::PageUp) {
            browser.pageSelection(-1);
            dirty = true;
        } else if (key == Key::PageDown) {
            browser.pageSelection(1);
            dirty = true;
        } else if (key == Key::Enter) {
            std::string chosen;
            if (browser.activateSelected(chosen)) {
                openModel(chosen);
            } else {
                dirty = true;
            }
        } else if (key == Key::Left || key == Key::Back) {
            browser.goParent();
            dirty = true;
        } else if (key == Key::Reset) {
            browser.reload();
            dirty = true;
        }
    };

    while (running && g_running) {
        // Pace continuous rendering at roughly 60 fps, but keep polling
        // often enough for responsive key handling.
        auto loop_now = std::chrono::steady_clock::now();
        int timeout_ms = 16;
        if (dirty || (mode == AppMode::Viewer && auto_rotate)) {
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
                    if (key == Key::Esc) {
                        esc_pressed = std::chrono::steady_clock::now();
                        esc_down = true;
                        exit_prompt_visible = false;
                        dirty = true;
                    } else if (mode == AppMode::Viewer) {
                        handleViewerPress(key);
                    } else {
                        handleBrowserPress(key);
                    }
                } else {
                    if (key == Key::Esc) {
                        esc_down = false;
                        exit_prompt_visible = false;
                        const auto duration =
                            std::chrono::steady_clock::now() - esc_pressed;
                        if (duration >= kExitHold) {
                            running = false;
                        } else if (mode == AppMode::Viewer) {
                            // Short Esc only closes the help overlay; the file
                            // browser is opened with B.
                            if (help) help = false;
                        }
                        // Always repaint so the prompt disappears immediately.
                        dirty = true;
                    } else if ((mode == AppMode::Viewer &&
                                isViewerRepeatable(key)) ||
                               (mode == AppMode::Browser &&
                                isBrowserRepeatable(key))) {
                        held[keyIndex(key)].active = false;
                    }
                }
            }
        }

        now = std::chrono::steady_clock::now();
        if (esc_down) {
            const auto held = now - esc_pressed;
            if (held >= kExitHold) {
                running = false;
            }
            if (held >= kExitPromptDelay) {
                exit_prompt_visible = true;
            }
            dirty = true;
        }

        const double dt =
            std::chrono::duration<double>(now - last_frame).count();
        last_frame = now;

        for (size_t i = 0; i < held.size(); ++i) {
            if (!held[i].active) continue;
            if (now < held[i].next_repeat) continue;

            const Key key = static_cast<Key>(i);
            if (mode == AppMode::Viewer) {
                if (held[i].pivot) {
                    applyPivotKey(key, camera);
                } else {
                    applyCameraKey(key, camera);
                }
                held[i].next_repeat = now + std::chrono::milliseconds(20);
            } else {
                if (key == Key::Up) {
                    browser.moveSelection(-1);
                } else if (key == Key::Down) {
                    browser.moveSelection(1);
                }
                held[i].next_repeat = now + std::chrono::milliseconds(70);
            }
            dirty = true;
        }

        if (mode == AppMode::Viewer && auto_rotate) {
            camera.yaw_deg += 40.0f * static_cast<float>(std::min(dt, 0.1));
            while (camera.yaw_deg > 180.0f) camera.yaw_deg -= 360.0f;
            dirty = true;
        }

        if (!toast.empty() && now >= toast_until) {
            toast.clear();
            dirty = true;
        }

        if (dirty) {
            float exit_progress = 0.0f;
            if (esc_down) {
                const double held =
                    std::chrono::duration<double>(now - esc_pressed).count();
                exit_progress =
                    static_cast<float>(held / 3.0);
            }

            const auto render_start = std::chrono::steady_clock::now();
            if (mode == AppMode::Browser) {
                drawBrowser(canvas, browser);
                if (exit_prompt_visible) drawExitPrompt(canvas, exit_progress);
                framebuffer.present(canvas);
                last_render_seconds = 0.0;
            } else if (have_model) {
                UiState ui;
                ui.mesh = &mesh;
                ui.camera = camera;
                ui.fps = fps;
                ui.auto_rotate = auto_rotate;
                ui.style = style;
                ui.wireframe = wireframe;
                ui.help = help;
                ui.toast = toast;

                renderFrame(canvas, mesh, camera, renderer, ui, options.no_ui,
                            wireframe ? RenderMode::Wireframe : style);
                if (exit_prompt_visible) drawExitPrompt(canvas, exit_progress);
                if (!options.no_ui) {
                    drawAxisGizmo(canvas, camera,
                                  activeViewport(canvas, options.no_ui));
                }
                framebuffer.present(canvas);

                const auto render_end = std::chrono::steady_clock::now();
                const double render_seconds =
                    std::chrono::duration<double>(render_end - render_start)
                        .count();
                last_render_seconds =
                    last_render_seconds <= 0.0
                        ? render_seconds
                        : last_render_seconds * 0.7 + render_seconds * 0.3;

                // Report the rate frames actually reach the panel. Idle gaps
                // are ignored so a paused viewer keeps its last active rate.
                const double interval =
                    std::chrono::duration<double>(render_end - last_present)
                        .count();
                if (interval > 0.0001 && interval < 0.25) {
                    const int sample =
                        static_cast<int>(std::lround(1.0 / interval));
                    fps = fps == 0 ? sample : (fps * 3 + sample) / 4;
                }
            }
            last_present = std::chrono::steady_clock::now();
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
    ui.style = options.style;
    ui.wireframe = options.wireframe;
    const RenderMode mode =
        options.wireframe ? RenderMode::Wireframe : options.style;

    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < options.bench_frames; ++frame) {
        camera.yaw_deg += 1.5f;
        if (camera.yaw_deg > 180.0f) camera.yaw_deg -= 360.0f;
        ui.camera = camera;
        renderFrame(canvas, mesh, camera, renderer, ui, options.no_ui, mode);
        if (!options.no_ui) {
            drawAxisGizmo(canvas, camera,
                          activeViewport(canvas, options.no_ui));
        }
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
    ui.style = options.style;
    ui.wireframe = options.wireframe;

    renderFrame(canvas, mesh, camera, renderer, ui, options.no_ui,
                options.wireframe ? RenderMode::Wireframe : options.style);
    if (!options.no_ui) {
        drawAxisGizmo(canvas, camera, activeViewport(canvas, options.no_ui));
    }

    if (!saveCanvasPng(canvas, options.shot_path)) {
        std::fprintf(stderr, "stlviewer: failed to write %s\n",
                     options.shot_path.c_str());
        return 1;
    }
    std::printf("wrote %s (%dx%d, %zu triangles)\n", options.shot_path.c_str(),
                canvas.width(), canvas.height(), mesh.triangleCount());
    return 0;
}

int listDirectory(const Options& options) {
    FileBrowser browser;
    std::string error;
    browser.init(options.list_path, configPath(), error);

    std::printf("%s\n", browser.directory().c_str());
    for (const FileEntry& entry : browser.entries()) {
        if (entry.is_directory) {
            std::printf("  DIR       %s\n", entry.name.c_str());
        } else {
            std::printf("  %8llu  %s\n",
                        static_cast<unsigned long long>(entry.size),
                        entry.name.c_str());
        }
    }
    if (!browser.error().empty()) {
        std::fprintf(stderr, "%s\n", browser.error().c_str());
        return 1;
    }
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

    if (options.list_dir) {
        return listDirectory(options);
    }

    const bool needs_model =
        !options.shot_path.empty() || options.bench_frames > 0;
    if (needs_model) {
        std::string path = options.file;
        if (path.empty()) path = findDefaultModel();
        if (path.empty()) {
            std::fprintf(stderr,
                         "stlviewer: no STL file given and none found\n");
            return 1;
        }
        Mesh mesh;
        if (!loadStl(path, mesh, error)) {
            std::fprintf(stderr, "stlviewer: %s: %s\n", path.c_str(),
                         error.c_str());
            return 1;
        }
        std::fprintf(stderr, "model: %s, %zu triangles\n", mesh.name.c_str(),
                     mesh.triangleCount());
        if (options.bench_frames > 0) return runBenchmark(options, mesh);
        return renderScreenshot(options, mesh);
    }

    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    return runInteractive(options);
}
