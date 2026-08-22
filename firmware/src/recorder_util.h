// Pure helpers for the SD recorder - no hardware includes, host-testable.
#ifndef RECORDER_UTIL_H
#define RECORDER_UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WAV_HEADER_BYTES 44

// Standard 44-byte PCM WAV header, mono 16-bit. Written with data_bytes=0 at
// open, then patched in place periodically and on close so a crash mid-session
// still leaves a playable file.
static inline void wav_header_fill(uint8_t h[WAV_HEADER_BYTES], uint32_t sample_rate, uint32_t data_bytes)
{
    uint32_t byte_rate = sample_rate * 2;
    uint32_t riff_size = 36 + data_bytes;
    memcpy(h, "RIFF", 4);
    h[4] = riff_size & 0xFF; h[5] = (riff_size >> 8) & 0xFF;
    h[6] = (riff_size >> 16) & 0xFF; h[7] = (riff_size >> 24) & 0xFF;
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0; // fmt chunk size
    h[20] = 1; h[21] = 0;                        // PCM
    h[22] = 1; h[23] = 0;                        // mono
    h[24] = sample_rate & 0xFF; h[25] = (sample_rate >> 8) & 0xFF;
    h[26] = (sample_rate >> 16) & 0xFF; h[27] = (sample_rate >> 24) & 0xFF;
    h[28] = byte_rate & 0xFF; h[29] = (byte_rate >> 8) & 0xFF;
    h[30] = (byte_rate >> 16) & 0xFF; h[31] = (byte_rate >> 24) & 0xFF;
    h[32] = 2; h[33] = 0;   // block align
    h[34] = 16; h[35] = 0;  // bits per sample
    memcpy(h + 36, "data", 4);
    h[40] = data_bytes & 0xFF; h[41] = (data_bytes >> 8) & 0xFF;
    h[42] = (data_bytes >> 16) & 0xFF; h[43] = (data_bytes >> 24) & 0xFF;
}

#ifndef REC_ROOT
#define REC_ROOT "/rec"
#endif

static inline void rec_session_path(char *out, size_t n, unsigned idx)
{
    snprintf(out, n, REC_ROOT "/S%04u", idx);
}

static inline void rec_segment_path(char *out, size_t n, const char *session, unsigned seg)
{
    snprintf(out, n, "%s/seg%02u", session, seg);
}

static inline void rec_frame_path(char *out, size_t n, const char *segment, unsigned frame)
{
    snprintf(out, n, "%s/f%06u.jpg", segment, frame);
}

static inline uint8_t disk_free_pct(uint64_t total_bytes, uint64_t used_bytes)
{
    if (total_bytes == 0) {
        return 0;
    }
    uint64_t free_b = total_bytes - used_bytes;
    return (uint8_t) ((free_b * 100) / total_bytes);
}

#endif // RECORDER_UTIL_H
