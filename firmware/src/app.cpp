#include "app.h"

#include <BLE2902.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>

#include "config.h" // Use config.h for all configurations
#include "esp_camera.h"
#include "esp_sleep.h"
#include "mic.h"
#include "opus_encoder.h"
#include "ota.h"
#include "sd_recorder.h"
#include "ui.h"

// GPIO21 doubles as the SD card's chip select: once the SD is mounted the
// status LED may never be driven again, or card transactions get corrupted.
// The external WS2812 (ui.cpp) takes over user signaling.
static inline void statusLedWrite(int v)
{
    if (!sd_recorder_mounted()) {
        digitalWrite(STATUS_LED_PIN, v);
    }
}

// Battery state
float batteryVoltage = 0.0f;
int batteryPercentage = 0;
unsigned long lastBatteryCheck = 0;

// Device power state
bool deviceActive = true;
device_state_t deviceState = DEVICE_BOOTING;

// LED state (the case button lives in ui.cpp on an analog node)
led_status_t ledMode = LED_BOOT_SEQUENCE;

// Gentle power optimization
unsigned long lastActivity = 0;
bool powerSaveMode = false;

// Light sleep optimization - saves ~15mA = adds 3-4 hours battery life
bool lightSleepEnabled = true;

// ---------------------------------------------------------------------------------
// BLE - Using config.h definitions
// ---------------------------------------------------------------------------------

// Device Information Service UUIDs
#define DEVICE_INFORMATION_SERVICE_UUID (uint16_t) 0x180A
#define MANUFACTURER_NAME_STRING_CHAR_UUID (uint16_t) 0x2A29
#define MODEL_NUMBER_STRING_CHAR_UUID (uint16_t) 0x2A24
#define FIRMWARE_REVISION_STRING_CHAR_UUID (uint16_t) 0x2A26
#define HARDWARE_REVISION_STRING_CHAR_UUID (uint16_t) 0x2A27
#define SERIAL_NUMBER_STRING_CHAR_UUID (uint16_t) 0x2A25

// Main Friend Service - using config.h UUIDs
static BLEUUID serviceUUID(OMI_SERVICE_UUID);
static BLEUUID photoDataUUID(PHOTO_DATA_UUID);
static BLEUUID photoControlUUID(PHOTO_CONTROL_UUID);
static BLEUUID audioDataUUID(AUDIO_DATA_UUID);
static BLEUUID audioCodecUUID(AUDIO_CODEC_UUID);

// OTA Service UUIDs
static BLEUUID otaServiceUUID(OTA_SERVICE_UUID);
static BLEUUID otaControlUUID(OTA_CONTROL_UUID);
static BLEUUID otaDataUUID(OTA_DATA_UUID);

// Characteristics
BLECharacteristic *photoDataCharacteristic;
BLECharacteristic *photoControlCharacteristic;
BLECharacteristic *batteryLevelCharacteristic;
BLECharacteristic *audioDataCharacteristic;
BLECharacteristic *audioCodecCharacteristic;
BLECharacteristic *otaControlCharacteristic;
BLECharacteristic *otaDataCharacteristic;

// Audio state
bool audioEnabled = true;
volatile bool audioSubscribed = false;
uint16_t audioPacketIndex = 0;

// State
bool connected = false;
bool isCapturingPhotos = false;
int captureInterval = 0; // Interval in ms
unsigned long lastCaptureTime = 0;

// Audio ring buffer for encoded packets
#define AUDIO_TX_BUFFER_SIZE (AUDIO_TX_RING_BUFFER_SIZE * (OPUS_OUTPUT_MAX_BYTES + 2))
static uint8_t audio_tx_buffer[AUDIO_TX_BUFFER_SIZE];
static volatile size_t audio_tx_write_pos = 0;
static volatile size_t audio_tx_read_pos = 0;
static uint8_t audio_packet_buffer[OPUS_OUTPUT_MAX_BYTES + AUDIO_PACKET_HEADER_SIZE];

size_t sent_photo_bytes = 0;
size_t sent_photo_frames = 0;
bool photoDataUploading = false;

// -------------------------------------------------------------------------
// Camera Frame
// -------------------------------------------------------------------------
camera_fb_t *fb = nullptr;
image_orientation_t current_photo_orientation = ORIENTATION_0_DEGREES;

// Forward declarations
void handlePhotoControl(int8_t controlValue);
void readBatteryLevel();
void updateBatteryService();
void updateLED();
void blinkLED(int count, int delayMs);
void enterPowerSave();
void exitPowerSave();
void shutdownDevice();
void enableLightSleep();

// Audio forward declarations
void onMicData(int16_t *data, size_t samples);
void onOpusEncoded(uint8_t *data, size_t len);
void processAudioTx();
void broadcastAudioPacket(uint8_t *data, size_t len);

// -------------------------------------------------------------------------
// LED Functions
// -------------------------------------------------------------------------
void updateLED()
{
    unsigned long now = millis();
    static unsigned long bootStartTime = 0;
    static unsigned long powerOffStartTime = 0;

    switch (ledMode) {
    case LED_BOOT_SEQUENCE:
        if (bootStartTime == 0)
            bootStartTime = now;

        // 5 quick blinks over 1.5 seconds total (inverted logic: HIGH=OFF, LOW=ON)
        if (now - bootStartTime < 1500) {
            int blinkPhase = ((now - bootStartTime) / 150) % 2;
            statusLedWrite( !blinkPhase);
        } else {
            statusLedWrite( HIGH); // OFF
            ledMode = LED_NORMAL_OPERATION;
            bootStartTime = 0;
        }
        break;

    case LED_POWER_OFF_SEQUENCE:
        if (powerOffStartTime == 0)
            powerOffStartTime = now;

        // 2 quick blinks over 800ms total (inverted logic: HIGH=OFF, LOW=ON)
        if (now - powerOffStartTime < 800) {
            int blinkPhase = ((now - powerOffStartTime) / 200) % 2;
            statusLedWrite( !blinkPhase);
        } else {
            statusLedWrite( HIGH); // OFF
            delay(100);
            shutdownDevice();
        }
        break;

    case LED_NORMAL_OPERATION:
    default:
        if (connected) {
            // Connected - LED solid ON
            statusLedWrite( LOW);
        } else {
            // Disconnected - LED slow blink (1 sec on, 1 sec off)
            int blinkPhase = (now / 1000) % 2;
            statusLedWrite( blinkPhase ? HIGH : LOW);
        }
        break;
    }
}

void blinkLED(int count, int delayMs)
{
    for (int i = 0; i < count; i++) {
        statusLedWrite( HIGH);
        delay(delayMs);
        statusLedWrite( LOW);
        delay(delayMs);
    }
}

// -------------------------------------------------------------------------
// Power Management
// -------------------------------------------------------------------------
void enterPowerSave()
{
    if (!powerSaveMode) {
        setCpuFrequencyMhz(MIN_CPU_FREQ_MHZ); // 40MHz for idle
        powerSaveMode = true;
    }
}

void exitPowerSave()
{
    if (powerSaveMode) {
        setCpuFrequencyMhz(NORMAL_CPU_FREQ_MHZ); // Back to 80MHz
        powerSaveMode = false;
    }
}

void app_register_activity()
{
    lastActivity = millis();
    if (powerSaveMode) {
        exitPowerSave();
    }
}

bool app_camera_busy()
{
    return photoDataUploading || fb != nullptr;
}

void enableLightSleep()
{
    if (!lightSleepEnabled || !connected || photoDataUploading) {
        return; // Don't sleep if disabled, not connected, or uploading
    }

    unsigned long now = millis();

    // Don't sleep if there was recent activity (within 5 seconds)
    if (now - lastActivity < 5000) {
        return;
    }

    unsigned long timeUntilNextPhoto = 0;

    if (isCapturingPhotos && captureInterval > 0) {
        unsigned long timeSinceLastPhoto = now - lastCaptureTime;
        if (timeSinceLastPhoto < captureInterval) {
            timeUntilNextPhoto = captureInterval - timeSinceLastPhoto;
        }
    }

    // Only sleep if we have at least 10 seconds until next photo
    if (timeUntilNextPhoto > 10000) {
        // Configure light sleep to wake on BLE events and timer
        unsigned long sleepTime = timeUntilNextPhoto - 5000;
        if (sleepTime > 15000)
            sleepTime = 15000;                           // Max 15 seconds
        esp_sleep_enable_timer_wakeup(sleepTime * 1000); // Wake 5s before photo or max 15s
        esp_light_sleep_start();
        lastActivity = millis(); // Update activity time after wake
    }
}

void shutdownDevice()
{
    Serial.println("Shutting down device...");

    // Stop audio
    mic_stop();

    // Stop photo capture
    isCapturingPhotos = false;

    // Disconnect BLE gracefully
    if (connected) {
        Serial.println("Disconnecting BLE...");
    }

    // Turn off LED (inverted logic)
    statusLedWrite( HIGH);

    // Enter deep sleep
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_1, 0); // Wake on button press
    Serial.println("Entering deep sleep...");
    delay(100);
    esp_deep_sleep_start();
}

// -------------------------------------------------------------------------
// Audio Functions
// -------------------------------------------------------------------------
void onMicData(int16_t *data, size_t samples)
{
    // Feed PCM data to Opus encoder
    opus_receive_pcm(data, samples);
    // Tee the same selected-mic stream into the local SD recording, if active
    sd_recorder_feed_audio(data, samples);
}

void onOpusEncoded(uint8_t *data, size_t len)
{
    // Store encoded data in TX ring buffer
    if (len > OPUS_OUTPUT_MAX_BYTES) {
        return;
    }

    // Write length (2 bytes) + data
    size_t packet_size = len + 2;
    size_t next_write = (audio_tx_write_pos + packet_size) % AUDIO_TX_BUFFER_SIZE;

    // Check for buffer overflow
    if ((audio_tx_write_pos < audio_tx_read_pos && next_write >= audio_tx_read_pos) ||
        (audio_tx_write_pos >= audio_tx_read_pos && next_write < audio_tx_write_pos &&
         next_write >= audio_tx_read_pos)) {
        // Buffer full, skip this packet
        return;
    }

    // Write length
    audio_tx_buffer[audio_tx_write_pos] = len & 0xFF;
    audio_tx_buffer[(audio_tx_write_pos + 1) % AUDIO_TX_BUFFER_SIZE] = (len >> 8) & 0xFF;

    // Write data
    for (size_t i = 0; i < len; i++) {
        audio_tx_buffer[(audio_tx_write_pos + 2 + i) % AUDIO_TX_BUFFER_SIZE] = data[i];
    }

    audio_tx_write_pos = next_write;
}

void broadcastAudioPacket(uint8_t *data, size_t len)
{
    if (!connected || !audioSubscribed || audioDataCharacteristic == nullptr) {
        return;
    }

    // Build packet: 2 bytes index + 1 byte sub-index + data
    audio_packet_buffer[0] = audioPacketIndex & 0xFF;
    audio_packet_buffer[1] = (audioPacketIndex >> 8) & 0xFF;
    audio_packet_buffer[2] = 0; // Sub-index (for fragmentation if needed)

    memcpy(audio_packet_buffer + AUDIO_PACKET_HEADER_SIZE, data, len);

    audioDataCharacteristic->setValue(audio_packet_buffer, len + AUDIO_PACKET_HEADER_SIZE);
    audioDataCharacteristic->notify();

    audioPacketIndex++;
}

void processAudioTx()
{
    if (!connected || !audioSubscribed) {
        return;
    }

    if (audioDataCharacteristic == nullptr) {
        return;
    }

    // Check if we have data in the ring buffer
    while (audio_tx_read_pos != audio_tx_write_pos) {
        // Read length
        uint16_t len =
            audio_tx_buffer[audio_tx_read_pos] | (audio_tx_buffer[(audio_tx_read_pos + 1) % AUDIO_TX_BUFFER_SIZE] << 8);

        if (len == 0 || len > OPUS_OUTPUT_MAX_BYTES) {
            // Invalid packet, skip
            audio_tx_read_pos = (audio_tx_read_pos + 2) % AUDIO_TX_BUFFER_SIZE;
            continue;
        }

        // Read data
        static uint8_t temp_data[OPUS_OUTPUT_MAX_BYTES];
        for (size_t i = 0; i < len; i++) {
            temp_data[i] = audio_tx_buffer[(audio_tx_read_pos + 2 + i) % AUDIO_TX_BUFFER_SIZE];
        }

        // Update read position
        audio_tx_read_pos = (audio_tx_read_pos + 2 + len) % AUDIO_TX_BUFFER_SIZE;

        // Send packet
        broadcastAudioPacket(temp_data, len);

        // Small delay to prevent BLE congestion
        delay(1);
    }
}

// -------------------------------------------------------------------------
// BLE Callbacks
// -------------------------------------------------------------------------
class ServerHandler : public BLEServerCallbacks
{
    void onConnect(BLEServer *server) override
    {
        connected = true;
        audioSubscribed = false;
        lastActivity = millis(); // Register activity - prevents sleep
        Serial.println(">>> BLE Client connected.");
        // Send current battery level on connect
        updateBatteryService();
    }
    void onDisconnect(BLEServer *server) override
    {
        connected = false;
        audioSubscribed = false;
        Serial.println("<<< BLE Client disconnected. Restarting advertising.");
        BLEDevice::startAdvertising();
    }
};

// Callback for Audio Data CCCD (Client Characteristic Configuration Descriptor)
class AudioCCCDCallback : public BLEDescriptorCallbacks
{
    void onWrite(BLEDescriptor *pDescriptor)
    {
        uint8_t *value = pDescriptor->getValue();
        if (value && pDescriptor->getLength() >= 2) {
            // Check notification bit (bit 0)
            if (value[0] & 0x01) {
                audioSubscribed = true;
                Serial.println("Audio notifications enabled");
            } else {
                audioSubscribed = false;
                Serial.println("Audio notifications disabled");
            }
        }
    }
};

class AudioDataCallback : public BLECharacteristicCallbacks
{
    void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code)
    {
        if (s == Status::SUCCESS_NOTIFY || s == Status::SUCCESS_INDICATE) {
            // Notification sent successfully
        }
    }

    void onRead(BLECharacteristic *pCharacteristic)
    {
        // Client read the characteristic
    }
};

class PhotoControlCallback : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *characteristic) override
    {
        if (characteristic->getLength() == 1) {
            int8_t received = characteristic->getData()[0];
            Serial.print("PhotoControl received: ");
            Serial.println(received);
            lastActivity = millis(); // Register activity - prevents sleep
            handlePhotoControl(received);
        }
    }
};

class OTAControlCallback : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pChar) override
    {
        std::string value = pChar->getValue();
        if (value.length() > 0) {
            ota_handle_command((uint8_t *) value.data(), value.length());
        }
    }

    void onRead(BLECharacteristic *pChar) override
    {
        uint8_t status[2] = {ota_get_status(), 0};
        pChar->setValue(status, 2);
    }
};

// -------------------------------------------------------------------------
// Battery Functions
// -------------------------------------------------------------------------
void readBatteryLevel()
{
    // Averaged without delays: this runs in the audio path, and back-to-back
    // conversions already settle. analogReadMilliVolts applies the eFuse ADC
    // calibration, which the raw (adc/4095)*3.3 formula throws away.
    uint32_t mvSum = 0;
    for (int i = 0; i < 10; i++) {
        mvSum += analogReadMilliVolts(BATTERY_ADC_PIN);
    }
    uint32_t nodeMv = mvSum / 10;

    // The button shares this analog node and shorts it to ground when held.
    // Keep the previous reading rather than reporting a dead battery.
    if (nodeMv < UI_BATTERY_VALID_MV) {
        return;
    }
    batteryVoltage = nodeMv * BATTERY_DIVIDER_NUM / 1000.0f;

    // Clamp voltage to reasonable range
    if (batteryVoltage > 5.0f)
        batteryVoltage = 5.0f;
    if (batteryVoltage < 2.5f)
        batteryVoltage = 2.5f;

    // Load-compensated battery calculation (accounts for voltage sag under load)
    float loadCompensatedMax = BATTERY_MAX_VOLTAGE;
    float loadCompensatedMin = BATTERY_MIN_VOLTAGE;

    // More accurate percentage calculation for load conditions
    if (batteryVoltage >= loadCompensatedMax) {
        batteryPercentage = 100;
    } else if (batteryVoltage <= loadCompensatedMin) {
        batteryPercentage = 0;
    } else {
        float range = loadCompensatedMax - loadCompensatedMin;
        batteryPercentage = (int) (((batteryVoltage - loadCompensatedMin) / range) * 100.0f);
    }

    // Smooth percentage changes to avoid jumpy readings
    static int lastBatteryPercentage = batteryPercentage;
    if (abs(batteryPercentage - lastBatteryPercentage) > 5) {
        batteryPercentage = lastBatteryPercentage + (batteryPercentage > lastBatteryPercentage ? 2 : -2);
    }
    lastBatteryPercentage = batteryPercentage;

    // Clamp percentage
    if (batteryPercentage > 100)
        batteryPercentage = 100;
    if (batteryPercentage < 0)
        batteryPercentage = 0;

    // Battery status with load info
    Serial.print("Battery: ");
    Serial.print(batteryVoltage);
    Serial.print("V (");
    Serial.print(batteryPercentage);
    Serial.print("%) [Load-compensated: ");
    Serial.print(loadCompensatedMin);
    Serial.print("V-");
    Serial.print(loadCompensatedMax);
    Serial.println("V]");
}

void updateBatteryService()
{
    if (batteryLevelCharacteristic) {
        uint8_t batteryLevel = (uint8_t) batteryPercentage;
        batteryLevelCharacteristic->setValue(&batteryLevel, 1);

        if (connected) {
            batteryLevelCharacteristic->notify();
        }
    }
}

// -------------------------------------------------------------------------
// configure_ble()
// -------------------------------------------------------------------------
void configure_ble()
{
    Serial.println("Initializing BLE...");
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerHandler());

    // Main service
    BLEService *service = server->createService(serviceUUID);

    // Audio Data characteristic (for streaming audio to app)
    audioDataCharacteristic = service->createCharacteristic(
        audioDataUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    BLE2902 *audioCcc = new BLE2902();
    audioCcc->setNotifications(true);
    audioCcc->setCallbacks(new AudioCCCDCallback());
    audioDataCharacteristic->addDescriptor(audioCcc);
    audioDataCharacteristic->setCallbacks(new AudioDataCallback());

    // Audio Codec characteristic (tells app which codec we're using)
    audioCodecCharacteristic = service->createCharacteristic(audioCodecUUID, BLECharacteristic::PROPERTY_READ);
    uint8_t codecId = opus_get_codec_id();
    audioCodecCharacteristic->setValue(&codecId, 1);

    // Photo Data characteristic
    photoDataCharacteristic = service->createCharacteristic(
        photoDataUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    BLE2902 *ccc = new BLE2902();
    ccc->setNotifications(true);
    photoDataCharacteristic->addDescriptor(ccc);

    // Photo Control characteristic
    photoControlCharacteristic = service->createCharacteristic(photoControlUUID, BLECharacteristic::PROPERTY_WRITE);
    photoControlCharacteristic->setCallbacks(new PhotoControlCallback());
    uint8_t controlValue = 0;
    photoControlCharacteristic->setValue(&controlValue, 1);

    // Battery Service
    BLEService *batteryService = server->createService(BATTERY_SERVICE_UUID);
    batteryLevelCharacteristic = batteryService->createCharacteristic(
        BATTERY_LEVEL_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    BLE2902 *batteryCcc = new BLE2902();
    batteryCcc->setNotifications(true);
    batteryLevelCharacteristic->addDescriptor(batteryCcc);

    // Set initial battery level
    readBatteryLevel();
    uint8_t initialBatteryLevel = (uint8_t) batteryPercentage;
    batteryLevelCharacteristic->setValue(&initialBatteryLevel, 1);

    // Device Information Service
    BLEService *deviceInfoService = server->createService(DEVICE_INFORMATION_SERVICE_UUID);
    BLECharacteristic *manufacturerNameCharacteristic =
        deviceInfoService->createCharacteristic(MANUFACTURER_NAME_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic *modelNumberCharacteristic =
        deviceInfoService->createCharacteristic(MODEL_NUMBER_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic *firmwareRevisionCharacteristic =
        deviceInfoService->createCharacteristic(FIRMWARE_REVISION_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic *hardwareRevisionCharacteristic =
        deviceInfoService->createCharacteristic(HARDWARE_REVISION_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic *serialNumberCharacteristic =
        deviceInfoService->createCharacteristic(SERIAL_NUMBER_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);

    manufacturerNameCharacteristic->setValue(MANUFACTURER_NAME);
    modelNumberCharacteristic->setValue(BLE_DEVICE_NAME);
    firmwareRevisionCharacteristic->setValue(FIRMWARE_VERSION_STRING);
    hardwareRevisionCharacteristic->setValue(HARDWARE_REVISION);

    // Generate serial number from ESP32 chip ID
    uint64_t chipId = ESP.getEfuseMac();
    char serialNumber[17];
    snprintf(serialNumber, sizeof(serialNumber), "%04X%08X", (uint16_t) (chipId >> 32), (uint32_t) chipId);
    serialNumberCharacteristic->setValue(serialNumber);

    // OTA Service
    BLEService *otaService = server->createService(otaServiceUUID);

    // OTA Control characteristic (for receiving commands and reading status)
    otaControlCharacteristic = otaService->createCharacteristic(
        otaControlUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    otaControlCharacteristic->setCallbacks(new OTAControlCallback());

    // OTA Data characteristic (for progress notifications)
    otaDataCharacteristic = otaService->createCharacteristic(
        otaDataUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    BLE2902 *otaCcc = new BLE2902();
    otaCcc->setNotifications(true);
    otaDataCharacteristic->addDescriptor(otaCcc);

    // Set OTA characteristics for the OTA module
    ota_set_characteristics(otaControlCharacteristic, otaDataCharacteristic);

    // Start services
    service->start();
    batteryService->start();
    deviceInfoService->start();
    otaService->start();

    // Start advertising
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(service->getUUID()); // Main service (fits in 31 bytes)
    advertising->setScanResponse(true);
    advertising->setMinPreferred(BLE_ADV_MIN_INTERVAL);
    advertising->setMaxPreferred(BLE_ADV_MAX_INTERVAL);
    BLEDevice::startAdvertising();

    Serial.println("BLE initialized and advertising started.");
}

// -------------------------------------------------------------------------
// Camera
// -------------------------------------------------------------------------
bool app_camera_ready();

bool take_photo()
{
    // The camera only exists during a video session in this build.
    if (!app_camera_ready()) {
        Serial.println("Photo requested but camera is powered down.");
        return false;
    }

    // Release previous buffer
    if (fb) {
        Serial.println("Releasing previous camera buffer...");
        esp_camera_fb_return(fb);
        fb = nullptr;
    }

    Serial.println("Capturing photo...");
    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Failed to get camera frame buffer!");
        return false;
    }
    Serial.print("Photo captured: ");
    Serial.print(fb->len);
    Serial.println(" bytes.");

    // Set fixed orientation for the captured photo
    current_photo_orientation = FIXED_IMAGE_ORIENTATION;
    Serial.println("Photo orientation set to 180 degrees (fixed).");

    lastActivity = millis(); // Register activity
    return true;
}

void handlePhotoControl(int8_t controlValue)
{
    if (controlValue == -1) {
        Serial.println("Received command: Single photo.");
        isCapturingPhotos = true;
        captureInterval = 0;
    } else if (controlValue == 0) {
        Serial.println("Received command: Stop photo capture.");
        isCapturingPhotos = false;
        captureInterval = 0;
    } else if (controlValue >= 5 && controlValue <= 300) {
        Serial.print("Received command: Start interval capture with parameter ");
        Serial.println(controlValue);

        // Use fixed interval from config for optimal battery life
        captureInterval = PHOTO_CAPTURE_INTERVAL_MS;
        Serial.print("Using configured interval: ");
        Serial.print(captureInterval / 1000);
        Serial.println(" seconds");

        isCapturingPhotos = true;
        lastCaptureTime = millis() - captureInterval;
    }
}

// -------------------------------------------------------------------------
// Camera lifecycle - powered only for the duration of a video session
// -------------------------------------------------------------------------
static bool camera_ready = false;

bool app_camera_ready()
{
    return camera_ready;
}

bool app_camera_start()
{
    if (camera_ready) {
        return true;
    }
    Serial.println("Initializing camera...");
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = CAMERA_XCLK_FREQ;

    // Use config.h camera settings optimized for battery life
    config.frame_size = CAMERA_FRAME_SIZE;
    config.pixel_format = PIXFORMAT_JPEG;
    // Two buffers: with one, a BLE photo upload holds the only frame for
    // seconds and every recorder grab blocks on a 4s camera timeout.
    config.fb_count = 2;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }
    camera_ready = true;
    Serial.println("Camera initialized successfully.");
    return true;
}

void app_camera_stop()
{
    if (!camera_ready) {
        return;
    }
    if (fb != nullptr) {
        esp_camera_fb_return(fb);
        fb = nullptr;
    }
    esp_camera_deinit();
    camera_ready = false;
    Serial.println("Camera powered down.");
}

// -------------------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------------------

// A small buffer for sending photo chunks over BLE
static uint8_t *s_compressed_frame_2 = nullptr;

void setup_app()
{
    Serial.begin(921600);
    Serial.println("Setup started...");

    // GPIO1 is not configured here: it is an analog node (battery divider +
    // button) owned by ui.cpp.
    pinMode(STATUS_LED_PIN, OUTPUT);

    // LED uses inverted logic: HIGH = OFF, LOW = ON
    statusLedWrite( HIGH);

    // Start LED boot sequence
    ledMode = LED_BOOT_SEQUENCE;

    // Case UI: button + WS2812 + battery gauge
    ui_init();

    // Power optimization from config.h
    setCpuFrequencyMhz(NORMAL_CPU_FREQ_MHZ);
    lastActivity = millis();

    configure_ble();

    // Allocate buffer for photo chunks (200 bytes + 2 for frame index)
    s_compressed_frame_2 = (uint8_t *) ps_calloc(202, sizeof(uint8_t));
    if (!s_compressed_frame_2) {
        Serial.println("Failed to allocate chunk buffer!");
    } else {
        Serial.println("Chunk buffer allocated successfully.");
    }

    // The camera belongs to the button-toggled video session only. Streaming
    // stills to the phone would contend for the frame buffer and burn power
    // for something this build has no use for.
    isCapturingPhotos = false;
    captureInterval = PHOTO_CAPTURE_INTERVAL_MS;
    lastCaptureTime = millis() - captureInterval;
    Serial.print("Default capture interval set to ");
    Serial.print(PHOTO_CAPTURE_INTERVAL_MS / 1000);
    Serial.println(" seconds.");

    // Initial battery reading
    // Battery voltage divider
    analogReadResolution(12);                           // optional: set 12-bit resolution
    analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db); // set attenuation for full 3.3V range

    readBatteryLevel();
    deviceState = DEVICE_ACTIVE;

    // Initialize audio subsystem
    Serial.println("Initializing audio subsystem...");
    if (opus_encoder_init()) {
        opus_set_callback(onOpusEncoded);

        if (mic_start()) {
            mic_set_callback(onMicData);
            Serial.println("Audio subsystem initialized successfully.");
        } else {
            Serial.println("Failed to start microphone!");
        }
    } else {
        Serial.println("Failed to initialize Opus encoder!");
    }

    Serial.println("Setup complete.");
    Serial.println("Light sleep optimization enabled for extended battery life.");
}

void loop_app()
{
    unsigned long now = millis();

    // Case UI state machine (battery check / record / bookmark / stop)
    ui_loop(now);
    sd_recorder_loop(now);

    // Update LED
    updateLED();

    // Process OTA updates
    ota_loop();

    // Process microphone data - always run to keep audio realtime
    if (audioEnabled && mic_is_running()) {
        mic_process();
        opus_process();
    }

    // Send audio packets over BLE - PRIORITY over photo
    if (connected && audioSubscribed) {
        processAudioTx();
    }

    // Check for power save mode (gentle optimization)
    if (!connected && !photoDataUploading && (now - lastActivity > IDLE_THRESHOLD_MS)) {
        enterPowerSave();
    } else if (connected || photoDataUploading) {
        if (powerSaveMode)
            exitPowerSave();
        lastActivity = now;
    }

    // Check battery level periodically
    if (now - lastBatteryCheck >= BATTERY_TASK_INTERVAL_MS) {
        readBatteryLevel();
        updateBatteryService();
        lastBatteryCheck = now;
    }

    // Force battery update on first connection
    static bool firstBatteryUpdate = true;
    if (connected && firstBatteryUpdate) {
        readBatteryLevel();
        updateBatteryService();
        firstBatteryUpdate = false;
    }

    // Check if it's time to capture a photo
    if (isCapturingPhotos && !photoDataUploading && connected) {
        if ((captureInterval == 0) || (now - lastCaptureTime >= (unsigned long) captureInterval)) {
            if (captureInterval == 0) {
                // Single shot if interval=0
                isCapturingPhotos = false;
            }
            Serial.println("Interval reached. Capturing photo...");
            if (take_photo()) {
                Serial.println("Photo capture successful. Starting upload...");
                photoDataUploading = true;
                sent_photo_bytes = 0;
                sent_photo_frames = 0;
                lastCaptureTime = now;
            }
        }
    }

    // If uploading, send chunks over BLE (interleave with audio - max 2 chunks per loop)
    static int photo_chunks_this_loop = 0;
    if (photoDataUploading && fb && photo_chunks_this_loop < 2) {
        // Yield to audio if audio buffer has data
        if (audioSubscribed && audio_tx_read_pos != audio_tx_write_pos) {
            photo_chunks_this_loop = 0; // Reset for next loop
        } else {
            photo_chunks_this_loop++;
        }
        size_t remaining = fb->len - sent_photo_bytes;
        if (remaining > 0) {
            size_t bytes_to_copy;
            if (sent_photo_frames == 0) {
                // First chunk: includes orientation metadata
                s_compressed_frame_2[0] = 0; // Frame index low byte
                s_compressed_frame_2[1] = 0; // Frame index high byte
                s_compressed_frame_2[2] = (uint8_t) current_photo_orientation;
                bytes_to_copy = (remaining > 199) ? 199 : remaining;
                memcpy(&s_compressed_frame_2[3], &fb->buf[sent_photo_bytes], bytes_to_copy);
                photoDataCharacteristic->setValue(s_compressed_frame_2, bytes_to_copy + 3);
            } else {
                // Subsequent chunks
                s_compressed_frame_2[0] = (uint8_t) (sent_photo_frames & 0xFF);
                s_compressed_frame_2[1] = (uint8_t) ((sent_photo_frames >> 8) & 0xFF);
                bytes_to_copy = (remaining > 200) ? 200 : remaining;
                memcpy(&s_compressed_frame_2[2], &fb->buf[sent_photo_bytes], bytes_to_copy);
                photoDataCharacteristic->setValue(s_compressed_frame_2, bytes_to_copy + 2);
            }
            photoDataCharacteristic->notify();

            sent_photo_bytes += bytes_to_copy;
            sent_photo_frames++;

            Serial.print("Uploading chunk ");
            Serial.print(sent_photo_frames);
            Serial.print(" (");
            Serial.print(bytes_to_copy);
            Serial.print(" bytes), ");
            Serial.print(remaining - bytes_to_copy);
            Serial.println(" bytes remaining.");

            lastActivity = now; // Register activity
        } else {
            // End of photo marker
            s_compressed_frame_2[0] = 0xFF;
            s_compressed_frame_2[1] = 0xFF;
            photoDataCharacteristic->setValue(s_compressed_frame_2, 2);
            photoDataCharacteristic->notify();
            Serial.println("Photo upload complete.");

            photoDataUploading = false;
            // Free camera buffer
            esp_camera_fb_return(fb);
            fb = nullptr;
            Serial.println("Camera frame buffer freed.");
            photo_chunks_this_loop = 0; // Reset counter
        }
    } else {
        photo_chunks_this_loop = 0; // Reset when not uploading
    }

    // Light sleep optimization - major power savings while maintaining BLE
    // Disable light sleep when audio is active
    if (!photoDataUploading && !audioSubscribed) {
        enableLightSleep();
    }

    // Adaptive delays for power saving (gentle optimization)
    if (photoDataUploading || audioSubscribed) {
        delay(5); // Fast during upload or audio streaming
    } else if (powerSaveMode) {
        delay(50); // Reduced delay with light sleep
    } else {
        delay(50); // Reduced delay with light sleep
    }
}
