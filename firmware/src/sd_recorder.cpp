#include "sd_recorder.h"

#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "esp_camera.h"

#include "app.h"
#include "config.h"
#include "recorder_util.h"

static bool mounted = false;
static bool recording = false;
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

bool sd_recorder_start()
{
    if (recording) {
        return true;
    }
    if (!mount_sd()) {
        return false;
    }
    SD.mkdir(REC_ROOT);
    // Next free session index
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
    char path[48];
    snprintf(path, sizeof(path), "%s/bookmarks.csv", session_dir);
    File bm = SD.open(path, FILE_WRITE);
    if (bm) {
        bm.println("millis,segment,frame");
        bm.close();
    }
    last_frame_ms = 0;
    last_patch_ms = millis();
    recording = true;
    Serial.printf("REC: started %s\n", session_dir);
    return true;
}

void sd_recorder_stop()
{
    if (!recording) {
        return;
    }
    recording = false;
    finalize_wav();
    Serial.printf("REC: stopped %s (%u segments)\n", session_dir, seg_idx);
}

void sd_recorder_bookmark()
{
    if (!recording) {
        return;
    }
    char path[48];
    snprintf(path, sizeof(path), "%s/bookmarks.csv", session_dir);
    File bm = SD.open(path, FILE_APPEND);
    if (bm) {
        bm.printf("%lu,%u,%u\n", (unsigned long) millis(), seg_idx, frame_idx);
        bm.close();
    }
    // Split: finalize the current segment's audio and roll to the next, so
    // each bookmark is also a clean file boundary for later demarcation.
    finalize_wav();
    seg_idx++;
    if (!open_segment()) {
        // Card full or write error: stop cleanly rather than silently
        // recording video with no audio for the rest of the session.
        Serial.println("REC: segment roll failed, stopping");
        recording = false;
        return;
    }
    Serial.printf("REC: bookmark -> %s\n", seg_dir);
}

void sd_recorder_feed_audio(int16_t *data, size_t samples)
{
    if (!recording || !wav_file) {
        return;
    }
    size_t bytes = samples * sizeof(int16_t);
    wav_file.write((const uint8_t *) data, bytes);
    wav_data_bytes += bytes;
}

void sd_recorder_loop(uint32_t now)
{
    if (!recording) {
        return;
    }
    app_register_activity(); // never idle-sleep mid-recording

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

    // Frame pump: audio has priority, so a late frame is skipped, not queued.
    // Skip entirely while the BLE photo path holds a frame buffer - grabbing
    // then would block this loop on the camera's 4s acquire timeout.
    if (now - last_frame_ms >= (1000 / VIDEO_FPS) && !app_camera_busy()) {
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
    }
}

bool sd_recorder_active()
{
    return recording;
}

bool sd_recorder_mounted()
{
    return mounted;
}

uint8_t sd_recorder_free_pct()
{
    if (!mounted) {
        return 0;
    }
    return disk_free_pct(SD.totalBytes(), SD.usedBytes());
}
