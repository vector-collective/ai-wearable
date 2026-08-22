#include "mic.h"

#include <driver/gpio.h>
#include <driver/i2s.h>

#include "config.h"

// Quad INMP441 capture: two standard-mode I2S buses, stereo each.
//   Bus 0 (I2S_NUM_0): case pair   -> A = left (L/R sel -> GND), B = right (L/R sel -> 3V3)
//   Bus 1 (I2S_NUM_1): rear/lapel  -> C = left (L/R sel -> GND), D = lapel, right (L/R sel -> 3V3)
// All four share the 16 kHz clock domain; each block we measure per-channel level,
// pick one source (lapel if present, else best case mic) and hand a mono block to
// the unchanged Opus/BLE pipeline via the existing callback.

#define MIC_FRAME_BYTES (2 * sizeof(int32_t)) // one stereo frame, 32-bit slots

enum mic_source { SRC_CASE_A = 0, SRC_CASE_B = 1, SRC_CASE_C = 2, SRC_LAPEL = 3 };
static const char *SRC_NAMES[] = {"CASE_A(front-up)", "CASE_B(out-up)", "CASE_C(rear)", "LAPEL"};

// Static variables
static volatile bool mic_running = false;
static bool bus1_ok = false;
static mic_data_handler audio_callback = nullptr;
static int32_t *bus0_buffer = nullptr;
static int32_t *bus1_buffer = nullptr;
static int16_t *mono_buffer = nullptr;

// Source-selection state
static int active_case = SRC_CASE_A;
static int candidate_case = SRC_CASE_A;
static int candidate_streak = 0;
static uint32_t lapel_hold_until = 0;
static int last_logged_source = -1;
static uint32_t last_stats_ms = 0;

static void *mic_alloc(size_t bytes)
{
    void *p = ps_malloc(bytes);
    if (p == nullptr) {
        p = malloc(bytes);
    }
    return p;
}

static bool install_bus(i2s_port_t port, int sck, int ws, int sd)
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = MIC_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        // 12 x 256 frames = ~190ms of buffering, comfortably more than the
        // 100ms read block. With only 4 buffers (64ms) any SD write or BLE
        // stall silently drops samples inside the driver.
        .dma_buf_count = 12,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = sck,
        .ws_io_num = ws,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = sd,
    };

    esp_err_t err = i2s_driver_install(port, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("I2S%d driver install failed: %s\n", (int) port, esp_err_to_name(err));
        return false;
    }

    err = i2s_set_pin(port, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("I2S%d set pin failed: %s\n", (int) port, esp_err_to_name(err));
        i2s_driver_uninstall(port);
        return false;
    }

    i2s_zero_dma_buffer(port);
    return true;
}

bool mic_start()
{
    if (mic_running) {
        Serial.println("Microphone already running");
        return true;
    }

    Serial.println("Initializing quad INMP441 I2S capture...");
    Serial.printf("  Bus0 (case A/B): SCK=GPIO%d WS=GPIO%d SD=GPIO%d\n", MIC_BUS0_SCK_PIN, MIC_BUS0_WS_PIN,
                  MIC_BUS0_SD_PIN);
    Serial.printf("  Bus1 (rear C / lapel D): SCK=GPIO%d WS=GPIO%d SD=GPIO%d\n", MIC_BUS1_SCK_PIN, MIC_BUS1_WS_PIN,
                  MIC_BUS1_SD_PIN);
    Serial.printf("  Sample Rate: %d Hz, bit shift: %d\n", MIC_SAMPLE_RATE, MIC_BIT_SHIFT);

    if (bus0_buffer == nullptr) {
        bus0_buffer = (int32_t *) mic_alloc(MIC_BUFFER_SAMPLES * MIC_FRAME_BYTES);
    }
    if (bus1_buffer == nullptr) {
        bus1_buffer = (int32_t *) mic_alloc(MIC_BUFFER_SAMPLES * MIC_FRAME_BYTES);
    }
    if (mono_buffer == nullptr) {
        mono_buffer = (int16_t *) mic_alloc(MIC_BUFFER_SAMPLES * sizeof(int16_t));
    }
    if (bus0_buffer == nullptr || bus1_buffer == nullptr || mono_buffer == nullptr) {
        Serial.println("Failed to allocate mic buffers!");
        return false;
    }

    if (!install_bus(I2S_NUM_0, MIC_BUS0_SCK_PIN, MIC_BUS0_WS_PIN, MIC_BUS0_SD_PIN)) {
        return false;
    }

    bus1_ok = install_bus(I2S_NUM_1, MIC_BUS1_SCK_PIN, MIC_BUS1_WS_PIN, MIC_BUS1_SD_PIN);
    if (bus1_ok) {
        // Pull the shared data line low so the lapel's (right) slot reads as
        // silence when no lapel is plugged in, giving clean presence detection.
        gpio_set_pull_mode((gpio_num_t) MIC_BUS1_SD_PIN, GPIO_PULLDOWN_ONLY);
    } else {
        Serial.println("Bus1 unavailable - continuing with case pair only");
    }

    mic_running = true;
    Serial.println("Microphone started successfully");
    return true;
}

void mic_stop()
{
    if (!mic_running) {
        return;
    }

    Serial.println("Stopping microphone...");

    i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    if (bus1_ok) {
        i2s_stop(I2S_NUM_1);
        i2s_driver_uninstall(I2S_NUM_1);
        bus1_ok = false;
    }

    mic_running = false;
    Serial.println("Microphone stopped");
}

bool mic_is_running()
{
    return mic_running;
}

void mic_set_callback(mic_data_handler callback)
{
    audio_callback = callback;
}

// Convert one 32-bit I2S slot (24-bit INMP441 data, MSB-aligned) to int16 with gain.
static inline int16_t slot_to_s16(int32_t raw)
{
    int32_t s = (raw >> MIC_BIT_SHIFT) * MIC_GAIN;
    if (s > 32767)
        s = 32767;
    if (s < -32768)
        s = -32768;
    return (int16_t) s;
}

void mic_process()
{
    if (!mic_running || bus0_buffer == nullptr) {
        return;
    }

    size_t bytes0 = 0, bytes1 = 0;
    esp_err_t err =
        i2s_read(I2S_NUM_0, bus0_buffer, MIC_BUFFER_SAMPLES * MIC_FRAME_BYTES, &bytes0, pdMS_TO_TICKS(40));
    if (err != ESP_OK || bytes0 == 0) {
        return;
    }
    size_t frames = bytes0 / MIC_FRAME_BYTES;

    // Bus 1 is advisory: a timeout or short read must never cost us bus 0
    // audio that has already been drained from its DMA and cannot be re-read.
    // On any shortfall, treat bus 1 as absent for this block only.
    bool bus1_valid = false;
    if (bus1_ok) {
        esp_err_t err1 =
            i2s_read(I2S_NUM_1, bus1_buffer, MIC_BUFFER_SAMPLES * MIC_FRAME_BYTES, &bytes1, pdMS_TO_TICKS(40));
        bus1_valid = (err1 == ESP_OK) && ((bytes1 / MIC_FRAME_BYTES) >= frames);
    }

    // Per-channel mean absolute level over this block.
    // Interleave order: [ch0, ch1] per frame; swap flags fix L/R if a board
    // revision or driver version delivers them reversed.
    uint64_t acc[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < frames; i++) {
        int16_t a = slot_to_s16(bus0_buffer[2 * i + (MIC_BUS0_SWAP_LR ? 1 : 0)]);
        int16_t b = slot_to_s16(bus0_buffer[2 * i + (MIC_BUS0_SWAP_LR ? 0 : 1)]);
        acc[SRC_CASE_A] += (a < 0) ? -a : a;
        acc[SRC_CASE_B] += (b < 0) ? -b : b;
        if (bus1_valid) {
            int16_t c = slot_to_s16(bus1_buffer[2 * i + (MIC_BUS1_SWAP_LR ? 1 : 0)]);
            int16_t d = slot_to_s16(bus1_buffer[2 * i + (MIC_BUS1_SWAP_LR ? 0 : 1)]);
            acc[SRC_CASE_C] += (c < 0) ? -c : c;
            acc[SRC_LAPEL] += (d < 0) ? -d : d;
        }
    }
    uint32_t level[4];
    for (int ch = 0; ch < 4; ch++) {
        level[ch] = (uint32_t) (acc[ch] / frames);
    }

    uint32_t now = millis();

    // Lapel presence: recent signal on channel D holds the lapel active so
    // natural pauses in speech don't bounce the source around. Compiled out
    // entirely when the pod isn't fitted (phase 1).
    if (MIC_LAPEL_FITTED && bus1_valid && level[SRC_LAPEL] > MIC_LAPEL_PRESENT_LEVEL) {
        lapel_hold_until = now + MIC_LAPEL_HOLD_MS;
    }
    bool lapel_active = MIC_LAPEL_FITTED && bus1_valid && ((int32_t) (now - lapel_hold_until) < 0);

    // If bus 1 dropped out this block, the rear mic's samples are stale -
    // never emit them, and re-home the incumbent onto a bus 0 channel.
    if (!bus1_valid && active_case == SRC_CASE_C) {
        active_case = (level[SRC_CASE_B] > level[SRC_CASE_A]) ? SRC_CASE_B : SRC_CASE_A;
        candidate_streak = 0;
    }

    // Case-mic selection with hysteresis: challenger must beat the incumbent
    // by MIC_SWITCH_RATIO for MIC_SWITCH_BLOCKS consecutive blocks.
    int best = SRC_CASE_A;
    int n_case = bus1_valid ? 3 : 2;
    for (int ch = 1; ch < n_case; ch++) {
        if (level[ch] > level[best]) {
            best = ch;
        }
    }
    // Only change over during near-silence: a mid-utterance splice joins two
    // different room responses, producing a click and a discontinuity that
    // corrupts speaker embeddings downstream.
    bool quiet = true;
    for (int ch = 0; ch < n_case; ch++) {
        if (level[ch] > MIC_SWITCH_SILENCE_LEVEL) {
            quiet = false;
        }
    }
    if (best != active_case && quiet &&
        level[best] * MIC_SWITCH_RATIO_DEN > level[active_case] * MIC_SWITCH_RATIO_NUM) {
        if (best == candidate_case) {
            candidate_streak++;
        } else {
            candidate_case = best;
            candidate_streak = 1;
        }
        if (candidate_streak >= MIC_SWITCH_BLOCKS) {
            active_case = best;
            candidate_streak = 0;
        }
    } else {
        candidate_streak = 0;
    }

    int source = lapel_active ? SRC_LAPEL : active_case;

    if (source != last_logged_source) {
        Serial.printf("MIC: source -> %s (levels A=%u B=%u C=%u D=%u)\n", SRC_NAMES[source], level[0], level[1],
                      level[2], level[3]);
        last_logged_source = source;
    }
    if (now - last_stats_ms >= MIC_STATS_INTERVAL_MS) {
        Serial.printf("MIC: levels A=%u B=%u C=%u D=%u active=%s lapel=%s\n", level[0], level[1], level[2], level[3],
                      SRC_NAMES[source], lapel_active ? "yes" : "no");
        last_stats_ms = now;
    }

    // Emit the selected channel as the mono stream.
    int32_t *src_buf = (source == SRC_CASE_A || source == SRC_CASE_B) ? bus0_buffer : bus1_buffer;
    int swap = (src_buf == bus0_buffer) ? MIC_BUS0_SWAP_LR : MIC_BUS1_SWAP_LR;
    int right_slot = (source == SRC_CASE_B || source == SRC_LAPEL) ? 1 : 0;
    int slot = swap ? (1 - right_slot) : right_slot;
    for (size_t i = 0; i < frames; i++) {
        mono_buffer[i] = slot_to_s16(src_buf[2 * i + slot]);
    }

    if (audio_callback != nullptr) {
        audio_callback(mono_buffer, frames);
    }
}
