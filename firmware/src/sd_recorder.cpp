#include "sd_recorder.h"

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "esp_camera.h"

#include "app.h"
#include "config.h"
#include "recorder_util.h"

// Everything that can stall - SD directory updates, multi-hundred-millisecond
// card write spikes, full-resolution camera grabs - happens in this task, not
// in loop_app(). Audio capture therefore never waits on storage, which is what
// lets the camera run at full resolution and quality.

enum rec_cmd { CMD_START = 1, CMD_STOP };

static StreamBufferHandle_t audio_stream = nullptr;
static QueueHandle_t cmd_queue = nullptr;
static TaskHandle_t rec_task = nullptr;

static volatile bool mounted = false;
static volatile bool recording = false; // reflects the task's real state
static volatile bool start_pending = false;
static volatile uint32_t audio_dropped_bytes = 0;
static volatile uint8_t cached_free_pct = 0;

// Task-owned state - only touched inside rec_task_fn
static unsigned session_idx = 0;
static unsigned seg_idx = 0;
static unsigned frame_idx = 0;
static char session_dir[24];
static char seg_dir[32];
static File wav_file;
static uint32_t wav_data_bytes = 0;
static uint32_t last_frame_ms = 0;
static uint32_t last_patch_ms = 0;

static bool mount_sd()
{
    if (mounted) {
        return true;
    }
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN);
    if (!SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ_HZ)) {
        Serial.println("REC: SD mount failed (no card?)");
        return false;
    }
    mounted = true;
    cached_free_pct = disk_free_pct(SD.totalBytes(), SD.usedBytes());
    Serial.printf("REC: SD mounted, %llu MB total, %llu MB used\n", SD.totalBytes() / (1024ULL * 1024ULL),
                  SD.usedBytes() / (1024ULL * 1024ULL));
    return true;
}

static bool open_wav(const char *dir)
{
    char path[48];
    snprintf(path, sizeof(path), "%s/audio.wav", dir);
    wav_file = SD.open(path, FILE_WRITE);
    if (!wav_file) {
        Serial.printf("REC: failed to open %s\n", path);
        return false;
    }
    uint8_t hdr[WAV_HEADER_BYTES];
    wav_header_fill(hdr, MIC_SAMPLE_RATE, 0);
    wav_file.write(hdr, WAV_HEADER_BYTES);
    wav_data_bytes = 0;
    return true;
}

static void finalize_wav()
{
    if (!wav_file) {
        return;
    }
    uint8_t hdr[WAV_HEADER_BYTES];
    wav_header_fill(hdr, MIC_SAMPLE_RATE, wav_data_bytes);
    wav_file.seek(0);
    wav_file.write(hdr, WAV_HEADER_BYTES);
    wav_file.close();
}

static bool open_segment()
{
    rec_segment_path(seg_dir, sizeof(seg_dir), session_dir, seg_idx);
    SD.mkdir(seg_dir);
    frame_idx = 0;
    return open_wav(seg_dir);
}

static bool session_start()
{
    if (!mount_sd()) {
        return false;
    }
    SD.mkdir(REC_ROOT);
    for (session_idx = 1; session_idx < 10000; session_idx++) {
        rec_session_path(session_dir, sizeof(session_dir), session_idx);
        if (!SD.exists(session_dir)) {
            break;
        }
    }
    SD.mkdir(session_dir);
    seg_idx = 1;
    if (!open_segment()) {
        return false;
    }
    if (!app_camera_start()) {
        Serial.println("REC: camera failed to start, audio-only session");
    }
    audio_dropped_bytes = 0;
    xStreamBufferReset(audio_stream);
    last_frame_ms = 0;
    last_patch_ms = millis();
    Serial.printf("REC: started %s\n", session_dir);
    return true;
}

static void session_stop()
{
    finalize_wav();
    app_camera_stop();
    cached_free_pct = disk_free_pct(SD.totalBytes(), SD.usedBytes());
    Serial.printf("REC: stopped %s (%u segments, %u audio bytes dropped)\n", session_dir, seg_idx,
                  (unsigned) audio_dropped_bytes);
}

static void drain_audio()
{
    static uint8_t chunk[2048];
    size_t n;
    while ((n = xStreamBufferReceive(audio_stream, chunk, sizeof(chunk), 0)) > 0) {
        if (wav_file) {
            wav_file.write(chunk, n);
            wav_data_bytes += n;
        }
    }
}

static void rec_task_fn(void *)
{
    for (;;) {
        uint8_t cmd = 0;
        // Wake on a command, or every 50ms to drain audio and pump frames.
        if (xQueueReceive(cmd_queue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
            switch (cmd) {
            case CMD_START:
                if (!recording) {
                    if (session_start()) {
                        recording = true;
                    }
                    start_pending = false;
                }
                break;
            case CMD_STOP:
                if (recording) {
                    recording = false;
                    drain_audio(); // flush what's still in flight
                    session_stop();
                }
                break;
            }
        }

        if (!recording) {
            continue;
        }

        app_register_activity(); // never idle-sleep mid-session
        drain_audio();

        uint32_t now = millis();

        // Crash safety: keep the WAV header sizes fresh
        if (now - last_patch_ms >= WAV_HEADER_PATCH_MS) {
            if (wav_file) {
                uint8_t hdr[WAV_HEADER_BYTES];
                wav_header_fill(hdr, MIC_SAMPLE_RATE, wav_data_bytes);
                size_t pos = wav_file.position();
                wav_file.seek(0);
                wav_file.write(hdr, WAV_HEADER_BYTES);
                wav_file.seek(pos);
                wav_file.flush();
            }
            last_patch_ms = now;
        }

        // Frame pump. Blocking here is fine - this is not the audio path.
        if (app_camera_ready() && (last_frame_ms == 0 || now - last_frame_ms >= VIDEO_FRAME_INTERVAL_MS) &&
            !app_camera_busy()) {
            camera_fb_t *frame = esp_camera_fb_get();
            if (frame) {
                char path[48];
                rec_frame_path(path, sizeof(path), seg_dir, frame_idx);
                File f = SD.open(path, FILE_WRITE);
                if (f) {
                    f.write(frame->buf, frame->len);
                    f.close();
                    frame_idx++;
                }
                esp_camera_fb_return(frame);
            }
            last_frame_ms = now;
            cached_free_pct = disk_free_pct(SD.totalBytes(), SD.usedBytes());
        }
    }
}

static bool ensure_task()
{
    if (rec_task != nullptr) {
        return true;
    }
    audio_stream = xStreamBufferCreate(REC_AUDIO_STREAM_BYTES, 1);
    cmd_queue = xQueueCreate(4, sizeof(uint8_t));
    if (audio_stream == nullptr || cmd_queue == nullptr) {
        Serial.println("REC: failed to allocate task resources");
        return false;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(rec_task_fn, "sd_rec", REC_TASK_STACK_SIZE, nullptr, REC_TASK_PRIORITY,
                                            &rec_task, REC_TASK_CORE);
    if (ok != pdPASS) {
        Serial.println("REC: failed to start recorder task");
        rec_task = nullptr;
        return false;
    }
    return true;
}

bool sd_recorder_start()
{
    if (recording || start_pending) {
        return true;
    }
    if (!ensure_task()) {
        return false;
    }
    start_pending = true;
    uint8_t cmd = CMD_START;
    if (xQueueSend(cmd_queue, &cmd, 0) != pdTRUE) {
        start_pending = false;
        return false;
    }
    // Optimistic: the LED confirms the request, and a mount failure is
    // reported by the task in the serial log and by sd_recorder_active().
    return true;
}

void sd_recorder_stop()
{
    if (cmd_queue == nullptr) {
        return;
    }
    uint8_t cmd = CMD_STOP;
    xQueueSend(cmd_queue, &cmd, 0);
}

void sd_recorder_feed_audio(int16_t *data, size_t samples)
{
    if (!recording || audio_stream == nullptr) {
        return;
    }
    size_t bytes = samples * sizeof(int16_t);
    // Never block the audio path: on a full buffer we count the loss instead.
    size_t sent = xStreamBufferSend(audio_stream, data, bytes, 0);
    if (sent < bytes) {
        audio_dropped_bytes += (bytes - sent);
    }
}

void sd_recorder_loop(uint32_t)
{
    // Intentionally empty: all work happens in rec_task_fn. Kept so app.cpp's
    // loop structure stays explicit about the recorder's existence.
}

bool sd_recorder_active()
{
    return recording;
}

bool sd_recorder_starting()
{
    return start_pending;
}

bool sd_recorder_mounted()
{
    return mounted;
}

uint8_t sd_recorder_free_pct()
{
    return mounted ? cached_free_pct : 0;
}
