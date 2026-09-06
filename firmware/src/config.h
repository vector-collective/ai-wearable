#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// BOARD CONFIGURATION - Must be defined before camera includes
// =============================================================================
#define CAMERA_MODEL_XIAO_ESP32S3 // Define camera model for Seeed Xiao ESP32S3
#define BOARD_HAS_PSRAM           // Enable PSRAM support
#define CONFIG_ARDUHAL_ESP_LOG    // Enable Arduino HAL logging

// =============================================================================
// DEVICE CONFIGURATION
// =============================================================================
#define BLE_DEVICE_NAME "OMI Glass"
#define FIRMWARE_VERSION_STRING "2.3.2"
#define HARDWARE_REVISION "ESP32-S3-v1.0"
#define MANUFACTURER_NAME "Based Hardware"

// =============================================================================
// POWER MANAGEMENT
// NOTE: the light-sleep path below is currently unreachable - loop_app()
// refreshes lastActivity on every iteration while connected, which is a
// precondition enableLightSleep() also requires. Left as-is deliberately:
// esp_light_sleep_start() gates APB out from under a running I2S peripheral,
// so re-enabling it means stopping the mics first.
// =============================================================================
// CPU Frequency Management - Aggressive power optimization
#define MAX_CPU_FREQ_MHZ 100   // Further reduced from 120MHz - still sufficient
#define MIN_CPU_FREQ_MHZ 40    // Ultra-low power for idle states
#define NORMAL_CPU_FREQ_MHZ 80 // Normal operation frequency (good balance)

// Sleep Management
#define LIGHT_SLEEP_DURATION_US 50000  // 50ms light sleep intervals
#define DEEP_SLEEP_THRESHOLD_MS 300000 // 5 minutes of inactivity triggers deep sleep
#define IDLE_THRESHOLD_MS 45000        // 45 seconds to enter power save mode (was 30s)

// Battery Configuration - single EEMB 1100mAh cell, swappable
#define BATTERY_MAX_VOLTAGE 4.2f      // 4.2V fully charged (under load)
#define BATTERY_MIN_VOLTAGE 3.2f      // 3.2V empty (under load)
#define BATTERY_CRITICAL_VOLTAGE 3.3f // Emergency shutdown voltage
#define BATTERY_LOW_VOLTAGE 3.4f      // Low battery warning
// (removed: upstream omiGlass divider ratio. This build reads the battery via
// analogReadMilliVolts * BATTERY_DIVIDER_NUM - see the CASE UI section.)

// Battery Monitoring - Extended intervals for power savings
#define BATTERY_REPORT_INTERVAL_MS 90000 // 1.5 minute reporting (was 60s)
#define BATTERY_TASK_INTERVAL_MS 20000   // 20 second internal checks (was 15s)
#define BATTERY_ADC_PIN 1                // GPIO1 (D0) - shared analog node: divider + button

// =============================================================================
// CAMERA CONFIGURATION - session-only, so quality is no longer power-limited
// =============================================================================
// The camera is initialized when a video session starts and deinitialized
// when it stops, so its cost is confined to sessions the user asked for.
// That, plus one frame per 30s and a recorder task that can block freely,
// removes every reason the upstream settings were throttled:
//   VGA/q25/6MHz  ->  ~15kB soft frames, sensor underclocked
//   UXGA/q10/20MHz -> ~250kB detailed frames, sensor at its normal clock
// At one frame per 30s that is ~30MB/hour: a 32GB card holds ~1000 hours.
#define CAMERA_FRAME_SIZE FRAMESIZE_UXGA // 1600x1200
#define CAMERA_JPEG_QUALITY 10           // 10-63, LOWER is better quality
#define CAMERA_XCLK_FREQ 20000000        // 20MHz - OV2640's normal clock
#define CAMERA_FB_IN_PSRAM CAMERA_FB_IN_PSRAM
#define CAMERA_GRAB_LATEST CAMERA_GRAB_LATEST

// Fixed Photo Capture Interval - Optimized for 6-8 hour operation
#define PHOTO_CAPTURE_INTERVAL_MS 30000 // Fixed 30 second interval
#define CAMERA_TASK_INTERVAL_MS 2000    // 2 second task check
#define CAMERA_TASK_STACK_SIZE 3072     // Reduced stack size
#define CAMERA_TASK_PRIORITY 2

// Camera Power Management - Reduce power cycling
#define CAMERA_POWER_DOWN_DELAY_MS 60000 // Power down camera after 60s idle (was 8s)

// =============================================================================
// IMAGE ORIENTATION
// =============================================================================
typedef enum {
    ORIENTATION_0_DEGREES = 0,   // Normal
    ORIENTATION_90_DEGREES = 1,  // Rotated right
    ORIENTATION_180_DEGREES = 2, // Upside down
    ORIENTATION_270_DEGREES = 3  // Rotated left
} image_orientation_t;

// The device is mounted upside down, so we need to rotate 180 degrees.
#define FIXED_IMAGE_ORIENTATION ORIENTATION_180_DEGREES

// =============================================================================
// BLE CONFIGURATION - Power optimized for extended battery life
// =============================================================================
#define BLE_MTU_SIZE 517            // Maximum MTU for efficiency
#define BLE_CHUNK_SIZE 500          // Safe chunk size for photo transfer
#define BLE_PHOTO_TRANSFER_DELAY 3  // Fast transfer for connection stability
#define BLE_TX_POWER ESP_PWR_LVL_N0 // Low power for 6+ hour battery life

// Power-optimized BLE Advertising - Longer intervals for power savings
#define BLE_ADV_MIN_INTERVAL 0x0140  // 200ms minimum (was 160ms)
#define BLE_ADV_MAX_INTERVAL 0x0280  // 400ms maximum (was 320ms)
#define BLE_ADV_TIMEOUT_MS 0         // Never stop advertising (always discoverable)
#define BLE_SLEEP_ADV_INTERVAL 45000 // Re-advertise every 45 seconds when not connected (was 30s)

// Connection Management - Stable connections with power optimization
#define BLE_CONNECTION_TIMEOUT_MS 0 // Never timeout connections (disable auto-disconnect)
#define BLE_TASK_INTERVAL_MS 20000  // 20 second connection check (was 15s)
#define BLE_TASK_STACK_SIZE 2048
#define BLE_TASK_PRIORITY 1

// Connection Parameters for Stable Connections with Power Optimization
#define BLE_CONN_MIN_INTERVAL 20 // 25ms minimum connection interval (was 20ms)
#define BLE_CONN_MAX_INTERVAL 40 // 50ms maximum connection interval (was 40ms)
#define BLE_CONN_LATENCY 0       // No latency for immediate response
#define BLE_CONN_TIMEOUT 800     // 8 second supervision timeout

// =============================================================================
// POWER STATES
// =============================================================================
typedef enum {
    POWER_STATE_ACTIVE,      // Normal operation - camera + BLE active
    POWER_STATE_POWER_SAVE,  // Reduced frequency, longer intervals
    POWER_STATE_LOW_BATTERY, // Minimal operation
    POWER_STATE_SLEEP        // Deep sleep mode
} power_state_t;

// =============================================================================
// TASK CONFIGURATION - Optimized stack sizes
// =============================================================================
#define BATTERY_TASK_STACK_SIZE 2048
#define BATTERY_TASK_PRIORITY 1
#define POWER_MANAGEMENT_TASK_STACK_SIZE 2048
#define POWER_MANAGEMENT_TASK_PRIORITY 0

// Status Reporting - Power optimized
#define STATUS_REPORT_INTERVAL_MS 120000 // 2 minutes (was 30 seconds)

// =============================================================================
// MICROPHONE CONFIGURATION - INMP441 I2S array (belt-worn build)
// =============================================================================
// Bus 0 carries the two case mics; bus 1 carries the rear case mic (left slot)
// and the detachable lapel pod (right slot).
// Bus 0 avoids GPIO7/8/9 (hardwired to the Sense board's microSD) and uses
// GPIO43/44, the old UART pins - free here because the console runs over
// native USB (ARDUINO_USB_CDC_ON_BOOT, stated explicitly in platformio.ini).
// GPIO1/GPIO2 belong to the case UI: see the CASE UI section below.
#define MIC_BUS0_SCK_PIN 6  // XIAO D5 - case pair bit clock
#define MIC_BUS0_WS_PIN 43  // XIAO D6 - case pair word select
#define MIC_BUS0_SD_PIN 44  // XIAO D7 - case pair data (A: L/R->GND, B: L/R->3V3)
#define MIC_BUS1_SCK_PIN 3 // XIAO D2 - rear/lapel bit clock
#define MIC_BUS1_WS_PIN 4  // XIAO D3 - rear/lapel word select
#define MIC_BUS1_SD_PIN 5  // XIAO D4 - rear/lapel data (C: L/R->GND, lapel D: L/R->3V3)

// Phase 1 ships without the lapel pod: the connector and firmware path are
// provisioned, the mic is not fitted. Set to 1 once the pod exists.
#ifndef MIC_LAPEL_FITTED
#define MIC_LAPEL_FITTED 0
#endif

#define MIC_SAMPLE_RATE 16000          // 16kHz sample rate
#define MIC_BUFFER_SAMPLES 1600        // 100ms block (16000 * 0.1)
#define MIC_BIT_SHIFT 14               // 32-bit slot -> int16 (24-bit data MSB-aligned; 14 = ~4x gain vs >>16)
#define MIC_GAIN 1                     // Extra multiplier after the shift (tune if quiet)
#define MIC_BUS0_SWAP_LR 0             // Set 1 if A/B appear swapped in the serial logs
#define MIC_BUS1_SWAP_LR 0             // Set 1 if C/lapel appear swapped in the serial logs
#define AUDIO_RING_BUFFER_SAMPLES 8000 // 500ms of audio data

// Source selection: lapel wins while it carries signal; otherwise the loudest
// case mic wins, with hysteresis so speech pauses don't cause flapping.
#define MIC_LAPEL_PRESENT_LEVEL 40 // Mean-abs level that marks the lapel "live"
#define MIC_LAPEL_HOLD_MS 5000     // Keep lapel selected this long after last signal
#define MIC_SWITCH_RATIO_NUM 3     // Challenger must exceed incumbent by 3/2
#define MIC_SWITCH_RATIO_DEN 2
#define MIC_SWITCH_BLOCKS 3         // ...for this many consecutive 100ms blocks
// Case mics only change over during near-silence. Switching mid-utterance
// splices two different room responses together: a broadband click that reads
// as a plosive, and a discontinuity that corrupts speaker embeddings. Below
// this level nobody is talking, so a changeover costs nothing.
#define MIC_SWITCH_SILENCE_LEVEL 60
#define MIC_STATS_INTERVAL_MS 10000 // Periodic level log for source analysis

// =============================================================================
// CASE UI - button + WS2812 RGB LED (shared pin with battery divider)
// =============================================================================
// GPIO2 (D1) drives the WS2812 data line and nothing else. An earlier revision
// shared it with the battery divider; that is electrically unsound - with the
// pad in ADC mode the divider holds the LED's data input at VBAT/2, squarely
// inside its forbidden band (0.3*VDD .. 0.7*VDD), so the latched colour is
// undefined. Power the LED from the regulated 3V3 rail, NOT the battery rail:
// WS2812B needs VIH >= 0.7*VDD, which a 3.3V GPIO cannot guarantee against a
// 4.2V supply.
#define UI_LED_PIN 2           // XIAO D1 - WS2812 DIN only
#define UI_LED_BRIGHTNESS 40   // 0-255; keep modest for current and glare
#define UI_LONG_PRESS_MS 1500  // deliberate hold: recording is a guarded, rarely-used control
#define UI_DEBOUNCE_MS 50

// Button and battery gauge share GPIO1 (D0) as a single ANALOG node. Both
// functions are high-impedance analog, so unlike the old GPIO2 arrangement
// they do not conflict:
//   BAT+ --[100k]-- GPIO1 --[100k]-- GND,  momentary switch across the lower
//   100k, and 100nF from GPIO1 to GND.
// Released -> VBAT/2 (~1.6-2.1V) = battery reading. Pressed -> ~0V.
// The 100nF both settles the ADC sample-and-hold and debounces the switch.
#define BATTERY_DIVIDER_NUM 2      // (Rtop+Rbot)/Rbot with 100k/100k
#define UI_BUTTON_PRESSED_MV 400   // node below this = pressed (floor is ~0V)
#define UI_BATTERY_VALID_MV 1200   // node above this = a trustworthy battery sample

// Recording heartbeat: a dim blink in the battery colour every 30s while a
// capture session is running, so a forgotten session is discoverable at a
// glance without the device being conspicuous. It never pre-empts a readout.
// The dim fraction is num/den of the already-brightness-scaled colour; raise
// UI_REC_HEARTBEAT_NUM on the bench if it is too faint through your diffuser.
// Readout blink counts. The count distinguishes the two things a quick press
// can do, since both use the same cool->warm colour spectrum:
//   5 blinks = battery readout (the device was idle)
//   2 blinks = capture stopped (the device was recording)
#define UI_BATTERY_BLINKS 5
#define UI_REC_STOP_BLINKS 2

#define UI_REC_HEARTBEAT_MS 30000
#define UI_REC_HEARTBEAT_NUM 1
#define UI_REC_HEARTBEAT_DEN 4

// =============================================================================
// SD RECORDER - Sense microSD slot (pins are hardwired on the daughterboard)
// =============================================================================
#define SD_SPI_SCK_PIN 7   // XIAO D8
#define SD_SPI_MISO_PIN 8  // XIAO D9
#define SD_SPI_MOSI_PIN 9  // XIAO D10
#define SD_CS_PIN 21       // shares the net with the onboard LED: once the SD
                           // is mounted, GPIO21 must never be driven as an LED
#define SD_SPI_FREQ_HZ 20000000
// The recorder runs in its own task so camera grabs and SD writes - which
// routinely stall for 100-500ms - can never block audio capture.
#define REC_TASK_STACK_SIZE 6144
#define REC_TASK_PRIORITY 2
#define REC_TASK_CORE 0
#define REC_AUDIO_STREAM_BYTES 32768 // ~1s of 16k mono PCM in flight
// Video is a button-toggled session layered on top of always-on audio.

// Photo chunks pushed over BLE per main-loop pass. A hard per-iteration cap:
// the loop breaks early whenever an audio packet is waiting, because audio is
// realtime and a photo is not.
#define PHOTO_CHUNKS_PER_LOOP 2
// One frame per 30s matches the SenseCam evidence base for photo-cued recall
// (~1 image / 30s) at ~1/150th the data rate and SD load of 5fps.
#define VIDEO_FRAME_INTERVAL_MS 30000
#define REC_ROOT "/rec"
#define WAV_HEADER_PATCH_MS 5000   // crash-safe header refresh interval

// =============================================================================
// OPUS CODEC CONFIGURATION
// =============================================================================
#define AUDIO_CODEC_ID 21              // Opus codec ID (matches Omi protocol)
#define OPUS_FRAME_SAMPLES 320         // 20ms frame @ 16kHz
#define OPUS_OUTPUT_MAX_BYTES 160      // Max encoded frame size
#define OPUS_BITRATE 32000             // 32kbps
#define OPUS_COMPLEXITY 3              // Encoding complexity (1-10)
#define OPUS_VBR 1                     // Variable bitrate enabled

// Audio BLE packet configuration
#define AUDIO_PACKET_HEADER_SIZE 3     // 2 bytes index + 1 byte sub-index
#define AUDIO_TX_RING_BUFFER_SIZE 16   // Number of encoded frames to buffer

// =============================================================================
// BLE UUID DEFINITIONS - OMI Protocol
// =============================================================================
#define OMI_SERVICE_UUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define AUDIO_DATA_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"
#define AUDIO_CODEC_UUID "19B10002-E8F2-537E-4F6C-D104768A1214"
#define PHOTO_DATA_UUID "19B10005-E8F2-537E-4F6C-D104768A1214"
#define PHOTO_CONTROL_UUID "19B10006-E8F2-537E-4F6C-D104768A1214"

// Battery Service UUID - Cast to uint16_t for BLE compatibility
#define BATTERY_SERVICE_UUID (uint16_t) 0x180F
#define BATTERY_LEVEL_UUID (uint16_t) 0x2A19

// OTA Service UUIDs
#define OTA_SERVICE_UUID "19B10010-E8F2-537E-4F6C-D104768A1214"
#define OTA_CONTROL_UUID "19B10011-E8F2-537E-4F6C-D104768A1214"  // Write commands, read status
#define OTA_DATA_UUID "19B10012-E8F2-537E-4F6C-D104768A1214"     // Notifications for progress

// OTA Commands (written to OTA_CONTROL_UUID)
#define OTA_CMD_SET_WIFI 0x01       // Set WiFi credentials: [cmd, ssid_len, ssid..., pass_len, pass...]
#define OTA_CMD_START_OTA 0x02      // Start OTA update: [cmd, url_len, url...]
#define OTA_CMD_CANCEL_OTA 0x03     // Cancel ongoing OTA
#define OTA_CMD_GET_STATUS 0x04     // Request current status
#define OTA_CMD_SET_URL 0x05        // Set firmware URL: [cmd, url_len, url...]

// OTA Status codes (notified via OTA_DATA_UUID)
#define OTA_STATUS_IDLE 0x00
#define OTA_STATUS_WIFI_CONNECTING 0x10
#define OTA_STATUS_WIFI_CONNECTED 0x11
#define OTA_STATUS_WIFI_FAILED 0x12
#define OTA_STATUS_DOWNLOADING 0x20      // Followed by progress byte (0-100)
#define OTA_STATUS_DOWNLOAD_COMPLETE 0x21
#define OTA_STATUS_DOWNLOAD_FAILED 0x22
#define OTA_STATUS_INSTALLING 0x30       // Followed by progress byte (0-100)
#define OTA_STATUS_INSTALL_COMPLETE 0x31
#define OTA_STATUS_INSTALL_FAILED 0x32
#define OTA_STATUS_REBOOTING 0x40
#define OTA_STATUS_ERROR 0xFF

// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT_MS 15000    // 15 seconds to connect
#define WIFI_MAX_SSID_LEN 32
#define WIFI_MAX_PASS_LEN 64
#define OTA_MAX_URL_LEN 256

// =============================================================================
// PIN DEFINITIONS (from camera_pins.h integration)
// =============================================================================
#ifdef CAMERA_MODEL_XIAO_ESP32S3
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39
#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13

// Power Button and LED Control
#define POWER_BUTTON_PIN 1 // GPIO1 (D0) - case button, read as ADC (see CASE UI)
#define STATUS_LED_PIN 21  // User LED (GPIO21) - status indicator
#endif

// =============================================================================
// POWER BUTTON & LED CONFIGURATION
// =============================================================================
// Button Configuration
#define BUTTON_DEBOUNCE_MS 50       // Button debounce time
#define POWER_OFF_PRESS_MS 2000     // Long press duration for power off (2 seconds)
#define BOOT_COMPLETE_DELAY_MS 3000 // LED indication during boot

// LED Status Patterns (in milliseconds)
#define LED_BOOT_BLINK_FAST 200     // Fast blink during boot
#define LED_BATTERY_LOW_BLINK 1000  // Slow blink for low battery
#define LED_SLEEP_BLINK 5000        // Very slow blink in deep sleep mode
#define LED_PHOTO_CAPTURE_FLASH 100 // Quick flash during photo capture

// Deep Sleep Configuration
#define DEEP_SLEEP_BUTTON_WAKEUP 1    // Enable button wake-up from deep sleep
#define POWER_OFF_SLEEP_DELAY_MS 1000 // Delay before entering deep sleep after power off

// Power Button States
typedef enum { BUTTON_IDLE, BUTTON_PRESSED, BUTTON_LONG_PRESS, BUTTON_RELEASED } button_state_t;

// LED Status Modes
typedef enum {
    LED_OFF,
    LED_ON,
    LED_BOOT_SEQUENCE,
    LED_NORMAL_OPERATION,
    LED_LOW_BATTERY,
    LED_PHOTO_CAPTURE,
    LED_POWER_OFF_SEQUENCE,
    LED_SLEEP_MODE
} led_status_t;

// Device Power States
typedef enum {
    DEVICE_BOOTING,
    DEVICE_ACTIVE,
    DEVICE_POWER_SAVE,
    DEVICE_LOW_BATTERY,
    DEVICE_POWERING_OFF,
    DEVICE_SLEEP
} device_state_t;

#endif // CONFIG_H
