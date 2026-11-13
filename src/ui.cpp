#include "ui.h"

#include <algorithm>

#include "app_icon.h"
#include "font.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kStatusHeight = 12;
constexpr int kHintHeight = 12;
constexpr int kTextLeft = 4;
constexpr int kTextTop = 2;
constexpr int kRowHeight = 11;

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

void fillRoundRect(Canvas& canvas, int x, int y, int w, int h, int radius,
                   uint16_t color) {
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w / 2, h / 2)));

    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius) {
            const float dy = radius - (row + 0.5f);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        } else if (row >= h - radius) {
            const float dy = (row + 0.5f) - (h - radius);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        }
        const int row_width = w - 2 * inset;
        if (row_width > 0) {
            canvas.fillRect(x + inset, y + row, row_width, 1, color);
        }
    }
}

void fillRoundRectGradient(Canvas& canvas, int x, int y, int w, int h,
                           int radius, uint16_t top, uint16_t bottom) {
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w / 2, h / 2)));

    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius) {
            const float dy = radius - (row + 0.5f);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        } else if (row >= h - radius) {
            const float dy = (row + 0.5f) - (h - radius);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        }
        const int row_width = w - 2 * inset;
        if (row_width <= 0) continue;
        const float t = h > 1 ? static_cast<float>(row) /
                                    static_cast<float>(h - 1)
                              : 0.0f;
        canvas.fillRect(x + inset, y + row, row_width, 1,
                        lerp565(top, bottom, t));
    }
}

void fillRoundRectBlend(Canvas& canvas, int x, int y, int w, int h,
                        int radius, uint16_t color, float alpha) {
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w / 2, h / 2)));

    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius) {
            const float dy = radius - (row + 0.5f);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        } else if (row >= h - radius) {
            const float dy = (row + 0.5f) - (h - radius);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        }
        for (int px = x + inset; px < x + w - inset; ++px) {
            canvas.blendPixel(px, y + row, color, alpha);
        }
    }
}

void drawSoftShadow(Canvas& canvas, int px, int py, int w, int h,
                    int radius) {
    struct Layer {
        int offset;
        float alpha;
    };
    static const Layer layers[] = {
        {5, 0.08f}, {4, 0.12f}, {3, 0.16f}, {2, 0.20f}, {1, 0.24f}};
    for (const Layer& layer : layers) {
        fillRoundRectBlend(canvas, px - layer.offset, py - layer.offset,
                           w + layer.offset * 2, h + layer.offset * 2,
                           radius + layer.offset, Theme::kDialogShadow,
                           layer.alpha);
    }
}

void drawLabelPill(Canvas& canvas, int center_x, int y, int height,
                   const std::string& label, uint16_t background) {
    const int text_width = canvas.textWidth(label);
    const int width = text_width + 11;
    const int x = center_x - width / 2;
    const int radius = height / 2;

    // Drop shadow and a dark rim make the key look raised off the render.
    fillRoundRectBlend(canvas, x + 1, y + 2, width - 1, height, radius,
                       0x0000, 0.28f);
    fillRoundRectBlend(canvas, x, y + 1, width, height, radius, 0x0000,
                       0.18f);

    const uint16_t rim = lerp565(background, 0x0000, 0.55f);
    const uint16_t top = lerp565(background, 0xFFFF, 0.70f);
    const uint16_t bottom = lerp565(background, 0x0000, 0.15f);

    fillRoundRect(canvas, x, y, width, height, radius, rim);
    fillRoundRectGradient(canvas, x + 1, y + 1, width - 2, height - 2,
                          radius - 1, top, bottom);

    canvas.text(x + (width - text_width) / 2, y + (height - 8 + 1) / 2, label,
                Theme::kPillText);
}

std::string humanSize(uint64_t bytes) {
    char buf[24];
    if (bytes >= 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1fM", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1fK", bytes / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%llu",
                      static_cast<unsigned long long>(bytes));
    }
    return buf;
}

const char* styleName(RenderMode style) {
    switch (style) {
        case RenderMode::Edges:
            return "EDGES";
        case RenderMode::Matcap:
            return "MATCAP";
        case RenderMode::Wireframe:
            return "WIRE";
        case RenderMode::Solid:
        default:
            return "SOLID";
    }
}

std::string displayPath(const std::string& path) {
    std::string shown = path;
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        const size_t home_len = std::strlen(home);
        if (shown.compare(0, home_len, home) == 0) {
            shown = "~" + shown.substr(home_len);
        }
    }
    if (shown.size() > 42) {
        shown = "..." + shown.substr(shown.size() - 39);
    }
    return shown;
}

}  // namespace

int statusBarHeight() { return kStatusHeight; }
int hintBarHeight() { return kHintHeight; }

Rect viewportRect(int canvas_width, int canvas_height) {
    Rect r;
    r.x = 0;
    r.y = kStatusHeight;
    r.w = canvas_width;
    // The bottom bar has no background; its labels float over the render, so
    // the viewport extends to the bottom edge.
    r.h = canvas_height - kStatusHeight;
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

    // Bottom overlay: no bar background, labels float over the render.
    const int hint_y = h - kHintHeight;

    // Dark 1px drop shadow keeps the colored axis text readable over light
    // parts of the model.
    auto draw_overlay_text = [&](int x, int y, const std::string& text,
                                 uint16_t color) {
        canvas.text(x + 1, y + 1, text, 0x0000);
        canvas.text(x, y, text, color);
    };

    int axis_x = kTextLeft;
    const int axis_y = hint_y + kTextTop;
    auto draw_axis = [&](char label, int value, uint16_t color) {
        char text[16];
        std::snprintf(text, sizeof(text), "%c%+d", label, value);
        draw_overlay_text(axis_x, axis_y, text, color);
        axis_x += canvas.textWidth(text) + FONT_6X8_WIDTH;
    };
    draw_axis('X', static_cast<int>(std::lround(state.camera.pitch_deg)),
              0xF800);
    draw_axis('Y', static_cast<int>(std::lround(state.camera.yaw_deg)),
              0x07E0);
    draw_axis('Z', static_cast<int>(std::lround(state.camera.roll_deg)),
              0x3C7F);

    const int pill_h = 12;
    const int pill_y = hint_y - 2;

    // The buttons and their key guides would collide with the help panel.
    if (!state.help) {
        // Auto-rotate toggle aligned with the "6" key (x=161).
        std::string auto_label = "AUTO";
        if (state.auto_mode == AutoRotateMode::X) {
            auto_label = "AUTO X";
        } else if (state.auto_mode == AutoRotateMode::Y) {
            auto_label = "AUTO Y";
        } else if (state.auto_mode == AutoRotateMode::Z) {
            auto_label = "AUTO Z";
        }
        drawLabelPill(canvas, 161, pill_y, pill_h, auto_label,
                      state.auto_mode == AutoRotateMode::Off
                          ? Theme::kPillBackground
                          : Theme::kPillActive);

        // Render style button aligned with the "7" key (x=217).
        const std::string style_label = styleName(state.style);
        drawLabelPill(canvas, 217, pill_y, pill_h, style_label,
                      Theme::kPillBackground);

        // Files button aligned with the "8" key (x=273).
        drawLabelPill(canvas, 273, pill_y, pill_h, "FILES",
                      Theme::kPillBackground);

    }

    // Transient toast (screenshot saved, etc.), styled like the other popups.
    if (!state.toast.empty()) {
        const std::string text = truncate(state.toast, 44);
        const int panel_w = std::min(w - 8, canvas.textWidth(text) + 16);
        const int panel_h = 18;
        const int radius = 5;
        const int px = (w - panel_w) / 2;
        const int py = pill_y - panel_h - 6;

        drawSoftShadow(canvas, px, py, panel_w, panel_h, radius);
        fillRoundRect(canvas, px, py, panel_w, panel_h, radius,
                      Theme::kDialogBorder);
        fillRoundRect(canvas, px + 1, py + 1, panel_w - 2, panel_h - 2,
                      radius - 1, Theme::kDialogBackground);
        canvas.text(px + (panel_w - canvas.textWidth(text)) / 2, py + 5, text,
                    Theme::kTextBright);
    }

    if (!state.help) return;

    // Help overlay: rounded elevated panel with a drop shadow.
    const int panel_w = 218;
    const int panel_h = 142;
    const int radius = 8;
    const int px = (w - panel_w) / 2;
    const int py = (h - panel_h) / 2;

    drawSoftShadow(canvas, px, py, panel_w, panel_h, radius);
    fillRoundRect(canvas, px, py, panel_w, panel_h, radius,
                  Theme::kDialogBorder);
    fillRoundRect(canvas, px + 1, py + 1, panel_w - 2, panel_h - 2,
                  radius - 1, Theme::kDialogBackground);

    canvas.text(px + 10, py + 7, "KanSTL", Theme::kAccent);
    drawTextRight(canvas, px + panel_w - 10, py + 7, "KEYS",
                  Theme::kTextDim);
    canvas.fillRect(px + 10, py + 20, panel_w - 20, 1, Theme::kBarBorder);

    struct HelpLine {
        const char* key;
        const char* action;
        uint16_t color;
    };
    static const HelpLine lines[] = {
        {"ARROWS", "ROTATE", Theme::kAccent},
        {"ALT+ARROWS", "MOVE CENTER", Theme::kAccent},
        {"ENTER/SPACE", "ZOOM IN / OUT", Theme::kAccent},
        {"R / 0", "RESET / FIT", Theme::kAccent},
        {"7", "STYLE + WIREFRAME", 0xFE60},
        {"6", "AUTO X / Y / Z", 0xFE60},
        {"HOLD 6", "STOP AUTO", 0xFE60},
        {"8", "OPEN FILE LIST", 0xFE60},
        {"S", "SCREENSHOT", 0xFE60},
        {"H", "CLOSE HELP", Theme::kTextBright},
        {"HOLD ESC", "EXIT", 0xF800},
    };

    int y = py + 27;
    for (const HelpLine& line : lines) {
        canvas.text(px + 10, y, line.key, line.color);
        canvas.text(px + 88, y, line.action, Theme::kTextBright);
        y += 10;
    }

    // App icon in the lower-right corner. Dark background pixels are stored as
    // 0 by the generator, so the cube floats directly on the panel.
    constexpr int kIconInset = 4;
    const int icon_x = px + panel_w - kIconInset - kAppIconSize;
    const int icon_y = py + panel_h - kIconInset - kAppIconSize;
    for (int iy = 0; iy < kAppIconSize; ++iy) {
        for (int ix = 0; ix < kAppIconSize; ++ix) {
            const uint16_t color = kAppIcon[iy * kAppIconSize + ix];
            if (color == 0) continue;
            canvas.pixel(icon_x + ix, icon_y + iy, color);
        }
    }
}

void drawBrowser(Canvas& canvas, const FileBrowser& browser) {
    const int w = canvas.width();
    const int h = canvas.height();
    const auto& entries = browser.entries();
    canvas.clear(Theme::kBackground);

    // Top bar: current directory and item count.
    canvas.fillRect(0, 0, w, kStatusHeight, Theme::kBarBackground);
    canvas.fillRect(0, kStatusHeight - 1, w, 1, Theme::kBarBorder);

    int item_count = static_cast<int>(entries.size());
    if (item_count > 0 && entries.front().name == "..") --item_count;
    char count_text[24];
    std::snprintf(count_text, sizeof(count_text), "%d ITEMS", item_count);

    canvas.text(kTextLeft, kTextTop, displayPath(browser.directory()),
                Theme::kTextBright);
    drawTextRight(canvas, w - 4, kTextTop, count_text, Theme::kText);

    // Rows.
    constexpr uint16_t kSelectedBg = 0x2945;
    constexpr uint16_t kSelectedText = 0xFFFF;
    constexpr uint16_t kSelectedDim = 0xCE79;

    const int list_top = kStatusHeight;
    for (int row = 0; row < kBrowserRows; ++row) {
        const int index = browser.scroll() + row;
        if (index >= static_cast<int>(entries.size())) break;

        const FileEntry& entry = entries[index];
        const int y = list_top + row * kRowHeight;
        const bool selected = index == browser.selection();

        uint16_t row_color = Theme::kTextBright;
        if (entry.is_directory) {
            row_color = entry.name == ".." ? Theme::kTextDim : Theme::kAccent;
        }

        if (selected) {
            canvas.fillRect(0, y, w, kRowHeight, kSelectedBg);
            canvas.fillRect(0, y, 2, kRowHeight, Theme::kAccent);
            canvas.text(3, y + 2, ">", Theme::kAccent);
        } else {
            canvas.fillRect(3, y + 2, 2, 7, row_color);
        }

        std::string name = entry.name;
        if (entry.is_directory && name != "..") name += "/";

        canvas.text(12, y + 2, truncate(name, 39),
                    selected ? kSelectedText : row_color);

        const std::string right =
            entry.is_directory ? std::string("DIR") : humanSize(entry.size);
        drawTextRight(canvas, w - 4, y + 2, right,
                      selected ? kSelectedDim : Theme::kTextDim);
    }

    bool has_files = false;
    for (const FileEntry& entry : entries) {
        if (!entry.is_directory) {
            has_files = true;
            break;
        }
    }
    if (!has_files) {
        const std::string empty = "NO .STL IN THIS FOLDER";
        canvas.text((w - canvas.textWidth(empty)) / 2, 78, empty,
                    Theme::kTextDim);
    }

    // Bottom bar: error message or key hints.
    const int hint_y = h - kHintHeight;
    canvas.fillRect(0, hint_y, w, kHintHeight, Theme::kBarBackground);
    canvas.fillRect(0, hint_y, w, 1, Theme::kBarBorder);

    if (!browser.error().empty()) {
        canvas.text(kTextLeft, hint_y + kTextTop,
                    truncate(browser.error(), 52), 0xF800);
    } else {
        int x = kTextLeft;
        const int y = hint_y + kTextTop;
        canvas.text(x, y, "ENTER", Theme::kAccent);
        x += 5 * FONT_6X8_WIDTH;
        canvas.text(x, y, " open  ", Theme::kText);
        x += 7 * FONT_6X8_WIDTH;
        canvas.text(x, y, "R", Theme::kAccent);
        x += FONT_6X8_WIDTH;
        canvas.text(x, y, " refresh", Theme::kText);
    }
}

void drawLoadingPopup(Canvas& canvas, const std::string& filename,
                      float progress) {
    const int w = canvas.width();
    const int h = canvas.height();
    const int panel_w = 248;
    const int panel_h = 78;
    const int radius = 8;
    const int px = (w - panel_w) / 2;
    const int py = (h - panel_h) / 2;

    drawSoftShadow(canvas, px, py, panel_w, panel_h, radius);
    fillRoundRect(canvas, px, py, panel_w, panel_h, radius,
                  Theme::kDialogBorder);
    fillRoundRect(canvas, px + 1, py + 1, panel_w - 2, panel_h - 2,
                  radius - 1, Theme::kDialogBackground);

    canvas.text(px + 12, py + 9, "LOADING", Theme::kAccent);
    canvas.text(px + 12, py + 25, truncate(filename, 36),
                Theme::kTextBright);

    const int bar_x = px + 12;
    const int bar_y = py + 45;
    const int bar_w = panel_w - 24;
    const int bar_h = 8;
    canvas.fillRect(bar_x, bar_y, bar_w, bar_h, Theme::kBarBorder);

    progress = std::max(0.0f, std::min(1.0f, progress));
    const int fill = static_cast<int>(bar_w * progress);
    if (fill > 0) {
        canvas.fillRect(bar_x, bar_y, fill, bar_h, Theme::kAccent);
    }

    char percent[16];
    std::snprintf(percent, sizeof(percent), "%d%%",
                  static_cast<int>(progress * 100.0f + 0.5f));
    drawTextRight(canvas, px + panel_w - 12, py + 58, percent,
                  Theme::kText);
}

void drawAxisGizmo(Canvas& canvas, const Camera& camera,
                   const Rect& viewport) {
    const int ox = viewport.x + viewport.w - 28;
    const int oy = viewport.y + 30;
    const float scale = 20.0f;

    const float yaw = camera.yaw_deg * 3.14159265358979323846f / 180.0f;
    const float pitch = camera.pitch_deg * 3.14159265358979323846f / 180.0f;
    const float roll = camera.roll_deg * 3.14159265358979323846f / 180.0f;
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cr = std::cos(roll);
    const float sr = std::sin(roll);

    auto axis = [&](float ax, float ay, float az, uint16_t color) {
        const float ax0 = ax * cr - ay * sr;
        const float ay0 = ax * sr + ay * cr;
        const float y1 = ay0 * cp - az * sp;
        const float z1 = ay0 * sp + az * cp;
        const float x2 = ax0 * cy + z1 * sy;
        const int ex = ox + static_cast<int>(std::lround(x2 * scale));
        const int ey = oy - static_cast<int>(std::lround(y1 * scale));
        canvas.line(ox, oy, ex, ey, color);
    };

    axis(1.0f, 0.0f, 0.0f, 0xF800);  // X red
    axis(0.0f, 1.0f, 0.0f, 0x07E0);  // Y green
    axis(0.0f, 0.0f, 1.0f, 0x3C7F);  // Z blue
    canvas.fillRect(ox, oy, 1, 1, Theme::kTextDim);
}

void drawExitPrompt(Canvas& canvas, float progress) {
    const int w = canvas.width();
    const int h = canvas.height();
    const int panel_w = 214;
    const int panel_h = 36;
    const int radius = 6;
    const int px = (w - panel_w) / 2;
    const int py = (h - panel_h) / 2;

    // Layered blue shadow, rounded border and raised panel.
    drawSoftShadow(canvas, px, py, panel_w, panel_h, radius);
    fillRoundRect(canvas, px, py, panel_w, panel_h, radius,
                  Theme::kDialogBorder);
    fillRoundRect(canvas, px + 1, py + 1, panel_w - 2, panel_h - 2,
                  radius - 1, Theme::kDialogBackground);

    const std::string title = "HOLD ESC 3S TO EXIT";
    canvas.text((w - canvas.textWidth(title)) / 2, py + 6, title,
                Theme::kTextBright);

    const int bar_x = px + 10;
    const int bar_y = py + 24;
    const int bar_w = panel_w - 20;
    canvas.fillRect(bar_x, bar_y, bar_w, 3, Theme::kBarBorder);

    progress = std::max(0.0f, std::min(1.0f, progress));
    const int fill = static_cast<int>(bar_w * progress);
    if (fill > 0) {
        canvas.fillRect(bar_x, bar_y, fill, 3, Theme::kAccent);
    }
}
