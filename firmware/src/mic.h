#ifndef MIC_H
#define MIC_H

#include <Arduino.h>
#include <stdint.h>

// Callback type for audio data
typedef void (*mic_data_handler)(int16_t *data, size_t samples);

// Device-tier voice events, raised from mic_process() on the main loop.
// CANDIDATE: the coarse novelty detector scored a segment of somebody
// else's speech that matches nothing heard recently. arg is the distance in
// dB x10 to the nearest known voice, or -10 when nothing was known.
enum { MIC_VOICE_CANDIDATE = 1 };
typedef void (*mic_voice_handler)(uint8_t event, int16_t arg);

/**
 * @brief Initialize and start the microphone
 * @return true if successful, false otherwise
 */
bool mic_start();

/**
 * @brief Stop the microphone
 */
void mic_stop();

/**
 * @brief Check if mic is running
 * @return true if running
 */
bool mic_is_running();

/**
 * @brief Set callback for mic data
 * @param callback Function to call when audio data is ready
 */
void mic_set_callback(mic_data_handler callback);

/**
 * @brief Set callback for device-tier voice events
 */
void mic_set_voice_callback(mic_voice_handler callback);

/**
 * @brief Who the last block belonged to: VOICE_SILENCE / VOICE_OWN / VOICE_OTHER
 */
int mic_voice_state();

/**
 * @brief Process mic data (call from main loop or task)
 */
void mic_process();

#endif // MIC_H
