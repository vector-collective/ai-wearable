#ifndef SD_RECORDER_H
#define SD_RECORDER_H

#include <Arduino.h>
#include <stdint.h>

// Button-toggled AV capture to the Sense board's microSD: a JPEG frame
// sequence plus a WAV of the same audio that is being streamed live, written
// into one folder per session. Audio capture itself is always on and is not
// affected by starting or stopping a session.

// Commands are queued to a background task; they return immediately and
// never block the audio path.
bool sd_recorder_start();                              // mounts SD on first use
void sd_recorder_stop();
void sd_recorder_loop(uint32_t now);                   // no-op; work runs in the task
void sd_recorder_feed_audio(int16_t *data, size_t samples);
bool sd_recorder_active();                             // session actually running
bool sd_recorder_starting();                           // start queued, not yet confirmed
bool sd_recorder_mounted();                            // true once SD ever mounted
bool sd_recorder_cs_claimed();                         // GPIO21 handed to the SD library: never drive it as an LED again
uint8_t sd_recorder_free_pct();                        // 0-100, 0 if no card

#endif // SD_RECORDER_H
