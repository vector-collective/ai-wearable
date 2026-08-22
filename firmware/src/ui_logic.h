// Pure button/LED decision logic - no hardware includes, host-testable.
#ifndef UI_LOGIC_H
#define UI_LOGIC_H

#include <stdint.h>

typedef enum {
    UI_ACT_NONE = 0,
    UI_ACT_BATTERY_CHECK, // idle: short press
    UI_ACT_REC_START,     // idle: long press (fires at threshold, while held)
    UI_ACT_REC_BOOKMARK,  // recording: short press
    UI_ACT_REC_STOP,      // recording: long press
} ui_action_t;

typedef struct {
    bool down;
    bool long_fired;
    bool recording;
    uint32_t press_ms;
    uint32_t last_edge_ms;
} ui_state_t;

// Feed with the current time and the raw pressed level each tick.
// Long presses fire once at the threshold without waiting for release;
// short presses fire on release. Edges inside the debounce window are ignored.
static inline ui_action_t ui_step(ui_state_t *s, uint32_t now, bool pressed, uint32_t long_ms, uint32_t debounce_ms)
{
    if (pressed && !s->down) {
        if (now - s->last_edge_ms < debounce_ms) {
            return UI_ACT_NONE;
        }
        s->down = true;
        s->long_fired = false;
        s->press_ms = now;
        s->last_edge_ms = now;
        return UI_ACT_NONE;
    }
    if (pressed && s->down && !s->long_fired && (now - s->press_ms) >= long_ms) {
        s->long_fired = true;
        if (s->recording) {
            s->recording = false;
            return UI_ACT_REC_STOP;
        }
        s->recording = true;
        return UI_ACT_REC_START;
    }
    if (!pressed && s->down) {
        if (now - s->last_edge_ms < debounce_ms) {
            return UI_ACT_NONE;
        }
        s->down = false;
        s->last_edge_ms = now;
        if (!s->long_fired) {
            return s->recording ? UI_ACT_REC_BOOKMARK : UI_ACT_BATTERY_CHECK;
        }
        s->long_fired = false;
    }
    return UI_ACT_NONE;
}

typedef struct {
    uint8_t r, g, b;
} ui_rgb_t;

// Battery gauge, cool -> warm: blue full, green good, yellow = plan a swap,
// orange = swap now ("change before red"), red = almost dead.
static inline ui_rgb_t ui_battery_color(uint16_t mv)
{
    if (mv >= 4000) return (ui_rgb_t){0, 80, 255};   // blue
    if (mv >= 3900) return (ui_rgb_t){0, 200, 180};  // cyan
    if (mv >= 3800) return (ui_rgb_t){0, 220, 40};   // green
    if (mv >= 3700) return (ui_rgb_t){240, 170, 0};  // yellow
    if (mv >= 3600) return (ui_rgb_t){255, 80, 0};   // orange
    return (ui_rgb_t){255, 0, 0};                    // red
}

// SD space remaining, same spectrum: blue = mostly free ... red = nearly full.
static inline ui_rgb_t ui_disk_color(uint8_t free_pct)
{
    if (free_pct >= 75) return (ui_rgb_t){0, 80, 255};
    if (free_pct >= 60) return (ui_rgb_t){0, 200, 180};
    if (free_pct >= 45) return (ui_rgb_t){0, 220, 40};
    if (free_pct >= 30) return (ui_rgb_t){240, 170, 0};
    if (free_pct >= 15) return (ui_rgb_t){255, 80, 0};
    return (ui_rgb_t){255, 0, 0};
}

#endif // UI_LOGIC_H
