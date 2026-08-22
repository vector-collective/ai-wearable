#include "ui.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "app.h"
#include "config.h"
#include "sd_recorder.h"
#include "ui_logic.h"

static Adafruit_NeoPixel pixel(1, UI_LED_PIN, NEO_GRB + NEO_KHZ800);
static ui_state_t ui_state = {};

// Non-blocking LED pattern engine
enum led_pattern { PAT_OFF, PAT_SOLID, PAT_BLINK };
static led_pattern pattern = PAT_OFF;
static ui_rgb_t pat_color;
static uint32_t pat_start = 0;
static uint32_t pat_solid_ms = 0;
static int pat_blinks = 0; // number of on-phases for PAT_BLINK

static void led_write(ui_rgb_t c)
{
    pixel.setPixelColor(0, pixel.Color(c.r, c.g, c.b));
    pixel.show();
}

static void led_off()
{
    led_write((ui_rgb_t){0, 0, 0});
    pattern = PAT_OFF;
}

static void start_solid(ui_rgb_t c, uint32_t ms, uint32_t now)
{
    pat_color = c;
    pat_solid_ms = ms;
    pat_start = now;
    pattern = PAT_SOLID;
    led_write(c);
}

static void start_blink(ui_rgb_t c, int count, uint32_t now)
{
    pat_color = c;
    pat_blinks = count;
    pat_start = now;
    pattern = PAT_BLINK;
}

// Raw millivolts at the shared analog node (GPIO1). Pressed pulls it to ~0V;
// released it sits at VBAT/2. One read serves both the button and the gauge.
static uint16_t node_read_mv()
{
    uint32_t acc = 0;
    for (int i = 0; i < 8; i++) {
        acc += analogReadMilliVolts(BATTERY_ADC_PIN);
    }
    return (uint16_t) (acc / 8);
}

static uint16_t last_battery_mv = 0;

void ui_init()
{
    // GPIO1 is an analog node (divider + button), not a digital input.
    analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
    pixel.begin();
    pixel.setBrightness(UI_LED_BRIGHTNESS);
    led_off();
}

static void run_pattern(uint32_t now)
{
    switch (pattern) {
    case PAT_SOLID:
        if (now - pat_start >= pat_solid_ms) {
            led_off();
        }
        break;
    case PAT_BLINK: {
        // 350ms on / 350ms off per blink. The main loop only samples this a
        // few times a second (mic block + SD writes), so a faster cadence
        // aliases and confirmation blinks get skipped entirely.
        uint32_t t = now - pat_start;
        int phase = t / 350;
        if (phase >= pat_blinks * 2) {
            led_off();
        } else {
            led_write((phase % 2 == 0) ? pat_color : (ui_rgb_t){0, 0, 0});
        }
        break;
    }
    default:
        break;
    }
}

void ui_loop(uint32_t now)
{
    uint16_t node_mv = node_read_mv();
    bool pressed = node_mv < UI_BUTTON_PRESSED_MV;
    if (node_mv >= UI_BATTERY_VALID_MV) {
        // Only trust the divider when the switch isn't shorting the node.
        last_battery_mv = node_mv * BATTERY_DIVIDER_NUM;
    }
    // The recorder runs asynchronously, so a session can fail to start (no
    // card) or stop itself (card full) after the fact. Resync and report.
    if (ui_state.recording && !sd_recorder_active() && !sd_recorder_starting()) {
        ui_state.recording = false;
        start_blink((ui_rgb_t){255, 0, 0}, 3, now);
    }

    ui_action_t act = ui_step(&ui_state, now, pressed, UI_LONG_PRESS_MS, UI_DEBOUNCE_MS);

    switch (act) {
    case UI_ACT_BATTERY_CHECK: {
        app_register_activity();
        Serial.printf("UI: battery %umV\n", last_battery_mv);
        start_solid(ui_battery_color(last_battery_mv), 2000, now);
        break;
    }
    case UI_ACT_REC_START: {
        app_register_activity();
        if (sd_recorder_start()) {
            // Single blink in battery color: confirms the request AND shows
            // whether the cell can carry a camera session. If the session
            // fails to come up, the resync above reports it.
            start_blink(ui_battery_color(last_battery_mv), 1, now);
        } else {
            ui_state.recording = false;
            start_blink((ui_rgb_t){255, 0, 0}, 3, now);
        }
        break;
    }
    case UI_ACT_REC_BOOKMARK:
        sd_recorder_bookmark();
        start_blink((ui_rgb_t){0, 200, 180}, 1, now);
        break;
    case UI_ACT_REC_STOP: {
        uint8_t free_pct = sd_recorder_free_pct();
        sd_recorder_stop();
        Serial.printf("UI: rec stop, SD %u%% free\n", free_pct);
        start_blink(ui_disk_color(free_pct), 2, now);
        break;
    }
    default:
        break;
    }

    run_pattern(now);
}
