#include "ui.h"

#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kStatusHeight = 12;
constexpr int kHintHeight = 12;
constexpr int kTextLeft = 4;
constexpr int kTextTop = 2;

std::string truncate(const std::string& value, size_t max_chars) {
    if (value.size() <= max_chars) return value;
    if (max_chars <= 3) return value.substr(0, max_chars);
    return value.substr(0, max_chars - 3) + "...";
}

std::string triangleText(size_t count) {
    char buf[32];
    if (count >= 100000) {
        std::snprintf(buf, sizeof(buf), "%.0fK", count / 1000.0);
    } else if (count >= 1000) {
        std::snprintf(buf, sizeof(buf), "%.1fK", count / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%zu", count);
    }
    return buf;
}

void drawTextRight(Canvas& canvas, int right_x, int y, const std::string& text,
                   uint16_t color) {
    canvas.text(right_x - canvas.textWidth(text), y, text, color);
}

}  // namespace

int statusBarHeight() { return kStatusHeight; }
int hintBarHeight() { return kHintHeight; }

Rect viewportRect(int canvas_width, int canvas_height) {
    Rect r;
    r.x = 0;
    r.y = kStatusHeight;
    r.w = canvas_width;
    r.h = canvas_height - kStatusHeight - kHintHeight;
    return r;
}

void drawUi(Canvas& canvas, const UiState& state) {
    const int w = canvas.width();
    const int h = canvas.height();

    // Top status bar.
    canvas.fillRect(0, 0, w, kStatusHeight, Theme::kBarBackground);
    canvas.fillRect(0, kStatusHeight - 1, w, 1, Theme::kBarBorder);

    const std::string name =
        state.mesh ? truncate(state.mesh->name, 30) : std::string("(no model)");
    canvas.text(kTextLeft, kTextTop, name, Theme::kTextBright);

    char info[32];
    if (state.fps > 0) {
        std::snprintf(info, sizeof(info), "%s TRI  %d FPS",
                      triangleText(state.mesh ? state.mesh->triangleCount() : 0)
                          .c_str(),
                      state.fps);
    } else {
        std::snprintf(info, sizeof(info), "%s TRI",
                      triangleText(state.mesh ? state.mesh->triangleCount() : 0)
                          .c_str());
    }
    drawTextRight(canvas, w - 4, kTextTop, info, Theme::kText);

    // Bottom hint bar.
    const int hint_y = h - kHintHeight;
    canvas.fillRect(0, hint_y, w, kHintHeight, Theme::kBarBackground);
    canvas.fillRect(0, hint_y, w, 1, Theme::kBarBorder);

    char view[64];
    std::snprintf(view, sizeof(view), "Y %+d  P %+d  Z %.2f  %s%s",
                  static_cast<int>(state.camera.yaw_deg),
                  static_cast<int>(state.camera.pitch_deg),
                  state.camera.zoom, state.wireframe ? "WIRE" : "SOLID",
                  state.auto_rotate ? "  AUTO" : "");
    canvas.text(kTextLeft, hint_y + kTextTop, view, Theme::kText);

    drawTextRight(canvas, w - 4, hint_y + kTextTop, "H HELP", Theme::kTextDim);

    // Transient toast (screenshot saved, etc.).
    if (!state.toast.empty()) {
        const std::string text = truncate(state.toast, 44);
        const int box_w = canvas.textWidth(text) + 10;
        const int box_x = (w - box_w) / 2;
        const int box_y = hint_y - 16;
        canvas.fillRect(box_x, box_y, box_w, 14, Theme::kBarBackground);
        canvas.rect(box_x, box_y, box_w, 14, Theme::kAccent);
        canvas.text(box_x + 5, box_y + 3, text, Theme::kTextBright);
    }

    if (!state.help) return;

    // Help overlay.
    const int panel_w = 218;
    const int panel_h = 124;
    const int px = (w - panel_w) / 2;
    const int py = (h - panel_h) / 2;
    canvas.fillRect(px, py, panel_w, panel_h, Theme::kOverlayBackground);
    canvas.rect(px, py, panel_w, panel_h, Theme::kOverlayBorder);
    canvas.text(px + 8, py + 6, "STL VIEWER", Theme::kAccent);

    static const char* lines[] = {
        "ARROWS    ROTATE",
        "ENTER     ZOOM IN",
        "SPACE     ZOOM OUT",
        "R         RESET VIEW",
        "0         FIT TO SCREEN",
        "W         WIRE / SOLID",
        "A         AUTO ROTATE",
        "S         SCREENSHOT",
        "H         CLOSE HELP",
        "ESC       HOLD TO EXIT",
    };
    int y = py + 20;
    for (const char* line : lines) {
        canvas.text(px + 8, y, line, Theme::kText);
        y += 10;
    }
}
