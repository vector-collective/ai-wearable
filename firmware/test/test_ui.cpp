// Host-side tests for the case UI state machine, color maps, and WAV header.
#include "../src/ui_logic.h"
#include "../src/recorder_util.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static ui_state_t s;
static uint32_t t;

static ui_action_t step(bool pressed, uint32_t advance)
{
    t += advance;
    return ui_step(&s, t, pressed, 700, 50);
}

int main()
{
    memset(&s, 0, sizeof(s));
    t = 1000;

    // Idle short press -> battery check on release
    assert(step(true, 100) == UI_ACT_NONE);
    assert(step(true, 200) == UI_ACT_NONE);   // held 200ms, under long threshold
    assert(step(false, 100) == UI_ACT_BATTERY_CHECK);
    printf("short press -> battery check OK\n");

    // Idle long press -> REC_START fires AT the threshold, not on release
    assert(step(true, 500) == UI_ACT_NONE);
    assert(step(true, 300) == UI_ACT_NONE);   // 300ms held
    assert(step(true, 500) == UI_ACT_REC_START); // 800ms held: fires
    assert(step(true, 500) == UI_ACT_NONE);   // still held: no repeat
    assert(step(false, 100) == UI_ACT_NONE);  // release after long: no short action
    printf("long press -> rec start OK\n");

    // Recording short press -> bookmark
    assert(step(true, 300) == UI_ACT_NONE);
    assert(step(false, 100) == UI_ACT_REC_BOOKMARK);
    printf("recording short press -> bookmark OK\n");

    // Recording long press -> stop
    assert(step(true, 300) == UI_ACT_NONE);
    assert(step(true, 800) == UI_ACT_REC_STOP);
    assert(step(false, 100) == UI_ACT_NONE);
    printf("recording long press -> stop OK\n");

    // Debounce: sub-50ms chatter after an edge is ignored
    assert(step(true, 300) == UI_ACT_NONE);   // press
    assert(step(false, 10) == UI_ACT_NONE);   // bounce, ignored
    assert(step(false, 100) == UI_ACT_BATTERY_CHECK); // real release
    printf("debounce OK\n");

    // Battery colors: cool = charged ... warm = low, red = almost dead
    assert(ui_battery_color(4150).b == 255);              // blue
    assert(ui_battery_color(3850).g == 220);              // green
    assert(ui_battery_color(3750).r == 240);              // yellow: change soon
    assert(ui_battery_color(3650).r == 255 && ui_battery_color(3650).g == 80); // orange: change now
    ui_rgb_t dead = ui_battery_color(3400);
    assert(dead.r == 255 && dead.g == 0 && dead.b == 0);  // red
    printf("battery color map OK\n");

    // Disk colors follow the same spectrum on free space
    assert(ui_disk_color(90).b == 255);
    assert(ui_disk_color(50).g == 220);
    ui_rgb_t full = ui_disk_color(5);
    assert(full.r == 255 && full.g == 0);
    printf("disk color map OK\n");

    // WAV header: canonical fields and patched sizes
    uint8_t h[WAV_HEADER_BYTES];
    wav_header_fill(h, 16000, 320000);
    assert(memcmp(h, "RIFF", 4) == 0 && memcmp(h + 8, "WAVEfmt ", 8) == 0);
    assert(h[22] == 1 && h[34] == 16);              // mono, 16-bit
    uint32_t sr = h[24] | (h[25] << 8) | (h[26] << 16) | ((uint32_t) h[27] << 24);
    uint32_t data = h[40] | (h[41] << 8) | (h[42] << 16) | ((uint32_t) h[43] << 24);
    uint32_t riff = h[4] | (h[5] << 8) | (h[6] << 16) | ((uint32_t) h[7] << 24);
    assert(sr == 16000 && data == 320000 && riff == 320036);
    printf("wav header OK\n");

    // Paths and free-space math
    char p[64], q[64], r[64];
    rec_session_path(p, sizeof(p), 7);
    assert(strcmp(p, "/rec/S0007") == 0);
    rec_segment_path(q, sizeof(q), p, 3);
    assert(strcmp(q, "/rec/S0007/seg03") == 0);
    rec_frame_path(r, sizeof(r), q, 42);
    assert(strcmp(r, "/rec/S0007/seg03/f000042.jpg") == 0);
    assert(disk_free_pct(100, 25) == 75);
    assert(disk_free_pct(0, 0) == 0);
    printf("paths + free pct OK\n");

    printf("ALL UI TESTS PASSED\n");
    return 0;
}
