#include "mic.h"

#include <driver/gpio.h>
#include <driver/i2s.h>

#include "config.h"
#include "voice_dsp.h"
#include "voice_logic.h"

// INMP441 capture: two standard-mode I2S buses, stereo each.
//   Bus 0 (I2S_NUM_0): A = left (L/R sel -> GND), B = right (L/R sel -> 3V3)
//   Bus 1 (I2S_NUM_1): C = left (L/R sel -> GND), lapel = right (L/R sel -> 3V3)
// All share the 16 kHz clock domain. Each 100 ms block, every channel that
// is present is converted to int16 and high-passed; we measure per-channel
// level, pick one source and hand its block to the unchanged Opus/BLE
// pipeline via the callback. The selected block also feeds the own-voice
// gate and the device-tier new-voice detector (voice_logic.h).
//
// Selection order: the lapel wins while it carries signal (when MIC_LAPEL_FITTED),
// otherwise the loudest case mic wins - but a case changeover is only permitted
// during near-silence. Switching mid-utterance splices two different room
// responses together: a broadband click that reads as a plosive, and a
// discontinuity that corrupts speaker embeddings downstream.

#define MIC_FRAME_BYTES (2 * sizeof(int32_t)) // one stereo frame, 32-bit slots

enum mic_source { SRC_CASE_A = 0, SRC_CASE_B = 1, SRC_CASE_C = 2, SRC_LAPEL = 3 };
static const char *SRC_NAMES[] = {"A", "B", "C", "LAPEL"};
static const char *VOICE_NAMES[] = {"silence", "own", "other"};

// Static variables
static volatile bool mic_running = false;
static bool bus1_ok = false;
static mic_data_handler audio_callback = nullptr;
static mic_voice_handler voice_callback = nullptr;
static int32_t *bus0_buffer = nullptr;
static int32_t *bus1_buffer = nullptr;
static int16_t *chan_buffer[4] = {nullptr, nullptr, nullptr, nullptr}; // converted + high-passed
static int16_t *mono_buffer = nullptr;

// Source-selection state
static int active_case = SRC_CASE_A;
static int candidate_case = SRC_CASE_A;
static int candidate_streak = 0;
#if MIC_LAPEL_FITTED
static uint32_t lapel_hold_until = 0;
#endif
static int last_logged_source = -1;
static uint32_t last_stats_ms = 0;

// Per-channel high-pass and the voice path
static hp1_t hp[4];
static voice_feat_state_t feat_state;
static voice_feat_t last_feat;
static voice_gate_t gate;
static novelty_t novelty;
static const voice_gate_cfg_t gate_cfg = {
    VOICE_SPEECH_RATIO, VOICE_SPEECH_MIN_LEVEL, VOICE_OWN_LEVEL, VOICE_OWN_TILT_MIN_DB,
    VOICE_HANGOVER_MS,  VOICE_FLOOR_RISE,       VOICE_FLOOR_FALL,
};
static const novelty_cfg_t novelty_cfg = {
    NOVELTY_MIN_BLOCKS, NOVELTY_SEGMENT_GAP_MS, NOVELTY_DIST_DB, NOVELTY_FORGET_MS,
};

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

    Serial.println("Initializing INMP441 I2S capture...");
    Serial.printf("  Bus0 (A/B): SCK=GPIO%d WS=GPIO%d SD=GPIO%d\n", MIC_BUS0_SCK_PIN, MIC_BUS0_WS_PIN,
                  MIC_BUS0_SD_PIN);
    Serial.printf("  Bus1 (C / lapel): SCK=GPIO%d WS=GPIO%d SD=GPIO%d\n", MIC_BUS1_SCK_PIN, MIC_BUS1_WS_PIN,
                  MIC_BUS1_SD_PIN);
    Serial.printf("  Sample Rate: %d Hz, bit shift: %d, high-pass: %d Hz\n", MIC_SAMPLE_RATE, MIC_BIT_SHIFT,
                  MIC_HIGHPASS_HZ);

    if (bus0_buffer == nullptr) {
        bus0_buffer = (int32_t *) mic_alloc(MIC_BUFFER_SAMPLES * MIC_FRAME_BYTES);
    }
    if (bus1_buffer == nullptr) {
        bus1_buffer = (int32_t *) mic_alloc(MIC_BUFFER_SAMPLES * MIC_FRAME_BYTES);
    }
    if (mono_buffer == nullptr) {
        mono_buffer = (int16_t *) mic_alloc(MIC_BUFFER_SAMPLES * sizeof(int16_t));
    }
    int n_chan = MIC_LAPEL_FITTED ? 4 : 3;
    bool alloc_ok = bus0_buffer != nullptr && bus1_buffer != nullptr && mono_buffer != nullptr;
    for (int ch = 0; ch < n_chan; ch++) {
        if (chan_buffer[ch] == nullptr) {
            chan_buffer[ch] = (int16_t *) mic_alloc(MIC_BUFFER_SAMPLES * sizeof(int16_t));
        }
        alloc_ok = alloc_ok && chan_buffer[ch] != nullptr;
    }
    if (!alloc_ok) {
        Serial.println("Failed to allocate mic buffers!");
        return false;
    }

    for (int ch = 0; ch < 4; ch++) {
        hp1_init(&hp[ch], (float) MIC_HIGHPASS_HZ, (float) MIC_SAMPLE_RATE);
    }
    voice_feat_init(&feat_state, (float) MIC_SAMPLE_RATE);
    voice_gate_init(&gate);
    novelty_init(&novelty);
    memset(&last_feat, 0, sizeof(last_feat));

    if (!install_bus(I2S_NUM_0, MIC_BUS0_SCK_PIN, MIC_BUS0_WS_PIN, MIC_BUS0_SD_PIN)) {
        return false;
    }

    bus1_ok = install_bus(I2S_NUM_1, MIC_BUS1_SCK_PIN, MIC_BUS1_WS_PIN, MIC_BUS1_SD_PIN);
    if (bus1_ok) {
        // Pull the shared data line low so the lapel's (right) slot reads as
        // silence when no lapel is plugged in, giving clean presence detection.
        gpio_set_pull_mode((gpio_num_t) MIC_BUS1_SD_PIN, GPIO_PULLDOWN_ONLY);
    } else {
        Serial.println("Bus1 unavailable - continuing with A/B only");
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

void mic_set_voice_callback(mic_voice_handler callback)
{
    voice_callback = callback;
}

int mic_voice_state()
{
    return gate.state;
}

// Convert one channel of a stereo 32-bit block to high-passed int16, and
// return its mean-abs level. The INMP441's 24 bits sit MSB-aligned in the
// slot; the shift keeps the top bits (see MIC_BIT_SHIFT in config.h).
static uint32_t convert_channel(int ch, const int32_t *stereo, int slot, size_t frames)
{
    hp1_t *f = &hp[ch];
    int16_t *out = chan_buffer[ch];
    float acc = 0.0f;
    for (size_t i = 0; i < frames; i++) {
        float s = (float) (stereo[2 * i + slot] >> MIC_BIT_SHIFT) * (float) MIC_GAIN;
        s = hp1_step(f, s);
        if (s > 32767.0f) {
            s = 32767.0f;
        } else if (s < -32768.0f) {
            s = -32768.0f;
        }
        // round, not truncate: a signal a hair under 1 LSB must not vanish
        int16_t v = (int16_t) (s >= 0.0f ? s + 0.5f : s - 0.5f);
        out[i] = v;
        acc += (v < 0) ? (float) -v : (float) v;
    }
    return (uint32_t) (acc / (float) frames);
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

    // Convert and high-pass every channel present this block, measuring
    // each one's mean-abs level. Interleave order is [ch0, ch1] per frame;
    // the swap flags fix L/R if a board revision or driver version delivers
    // them reversed.
    uint32_t level[4] = {0, 0, 0, 0};
    level[SRC_CASE_A] = convert_channel(SRC_CASE_A, bus0_buffer, MIC_BUS0_SWAP_LR ? 1 : 0, frames);
    level[SRC_CASE_B] = convert_channel(SRC_CASE_B, bus0_buffer, MIC_BUS0_SWAP_LR ? 0 : 1, frames);
    if (bus1_valid) {
        level[SRC_CASE_C] = convert_channel(SRC_CASE_C, bus1_buffer, MIC_BUS1_SWAP_LR ? 1 : 0, frames);
#if MIC_LAPEL_FITTED
        level[SRC_LAPEL] = convert_channel(SRC_LAPEL, bus1_buffer, MIC_BUS1_SWAP_LR ? 0 : 1, frames);
#endif
    }

    uint32_t now = millis();

#if MIC_LAPEL_FITTED
    // Lapel presence: recent signal on the lapel channel holds the lapel
    // active so natural pauses in speech don't bounce the source around.
    if (bus1_valid && level[SRC_LAPEL] > MIC_LAPEL_PRESENT_LEVEL) {
        lapel_hold_until = now + MIC_LAPEL_HOLD_MS;
    }
    bool lapel_active = bus1_valid && ((int32_t) (now - lapel_hold_until) < 0);
#else
    // Pod not fitted: the lapel slot is never sampled, scored, or selected, so
    // an unconnected input cannot influence anything.
    const bool lapel_active = false;
#endif

    // If bus 1 dropped out this block, C's samples are stale - never emit
    // them, and re-home the incumbent onto a bus 0 channel.
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

    // Emit the selected channel as the mono stream.
    memcpy(mono_buffer, chan_buffer[source], frames * sizeof(int16_t));

    // Who is talking, and is it anyone new. Runs on the stream that leaves
    // the device, so it sees exactly what the pipeline will.
    voice_feat_block(&feat_state, mono_buffer, frames, &last_feat);
    int vstate = voice_gate_step(&gate, &gate_cfg, now, &last_feat);
    bool scored_before = novelty.seg_scored;
    if (novelty_step(&novelty, &novelty_cfg, now, vstate, gate.speech, &last_feat)) {
        int16_t arg = (novelty.last_dist < 0.0f) ? -10 : (int16_t) (novelty.last_dist * 10.0f + 0.5f);
        Serial.printf("VOICE: new-voice candidate dist=%.1f slot=%d live=%d\n", novelty.last_dist, novelty.last_match,
                      novelty_live_count(&novelty, &novelty_cfg, now));
        if (voice_callback != nullptr) {
            voice_callback(MIC_VOICE_CANDIDATE, arg);
        }
    } else if (!scored_before && novelty.seg_scored) {
        Serial.printf("VOICE: segment matched slot %d dist=%.1f\n", novelty.last_match, novelty.last_dist);
    }

    if (source != last_logged_source) {
#if MIC_LAPEL_FITTED
        Serial.printf("MIC: source -> %s (levels A=%u B=%u C=%u D=%u)\n", SRC_NAMES[source], level[0], level[1],
                      level[2], level[3]);
#else
        Serial.printf("MIC: source -> %s (levels A=%u B=%u C=%u)\n", SRC_NAMES[source], level[0], level[1], level[2]);
#endif
        last_logged_source = source;
    }
    if (now - last_stats_ms >= MIC_STATS_INTERVAL_MS) {
#if MIC_LAPEL_FITTED
        Serial.printf("MIC: levels A=%u B=%u C=%u D=%u active=%s lapel=%s\n", level[0], level[1], level[2], level[3],
                      SRC_NAMES[source], lapel_active ? "yes" : "no");
#else
        Serial.printf("MIC: levels A=%u B=%u C=%u active=%s\n", level[0], level[1], level[2], SRC_NAMES[source]);
#endif
        // The calibration line: read level and tilt while you talk, then
        // while someone talks to you, and set VOICE_OWN_LEVEL / _TILT_MIN_DB
        // between them (config.h).
        Serial.printf("VOICE: %s level=%.0f floor=%.1f tilt=%+.1f bands=%.0f/%.0f/%.0f/%.0f/%.0f known=%d\n",
                      VOICE_NAMES[vstate], last_feat.level, gate.floor, gate.tilt_avg, last_feat.band_db[0],
                      last_feat.band_db[1], last_feat.band_db[2], last_feat.band_db[3], last_feat.band_db[4],
                      novelty_live_count(&novelty, &novelty_cfg, now));
        last_stats_ms = now;
    }

    if (audio_callback != nullptr) {
        audio_callback(mono_buffer, frames);
    }
}
