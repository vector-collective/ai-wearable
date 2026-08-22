#ifndef SD_RECORDER_H
#define SD_RECORDER_H

#include <Arduino.h>
#include <stdint.h>

// Button-gated local recording to the Sense board's microSD: JPEG frame
// sequence + WAV of the selected mic stream, per-session folders, segment
// split + bookmark log on demand.

bool sd_recorder_start();                              // mounts SD on first use
void sd_recorder_stop();
void sd_recorder_bookmark();                           // logs + splits segment
void sd_recorder_loop(uint32_t now);                   // frame pump, header patch
void sd_recorder_feed_audio(int16_t *data, size_t samples);
bool sd_recorder_active();
bool sd_recorder_mounted();                            // true once SD ever mounted (GPIO21 is then off-limits as LED)
uint8_t sd_recorder_free_pct();                        // 0-100, 0 if no card

#endif // SD_RECORDER_H
