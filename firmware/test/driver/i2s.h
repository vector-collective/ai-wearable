// Host-side stub of ESP-IDF 4.4 legacy I2S driver (signatures mirrored from
// components/driver/include/driver/i2s.h). i2s_read feeds synthetic audio
// controlled by the test via g_chan_amp[port][channel].
#ifndef I2S_STUB_H
#define I2S_STUB_H

#include <cstdint>
#include <cstring>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
inline const char *esp_err_to_name(esp_err_t) { return "ESP_OK"; }

typedef enum { I2S_NUM_0 = 0, I2S_NUM_1 = 1, I2S_NUM_MAX } i2s_port_t;

typedef enum {
    I2S_MODE_MASTER = 1,
    I2S_MODE_SLAVE = 2,
    I2S_MODE_TX = 4,
    I2S_MODE_RX = 8,
    I2S_MODE_PDM = 64,
} i2s_mode_t;

typedef enum {
    I2S_BITS_PER_SAMPLE_16BIT = 16,
    I2S_BITS_PER_SAMPLE_24BIT = 24,
    I2S_BITS_PER_SAMPLE_32BIT = 32,
} i2s_bits_per_sample_t;

typedef enum {
    I2S_CHANNEL_FMT_RIGHT_LEFT,
    I2S_CHANNEL_FMT_ALL_RIGHT,
    I2S_CHANNEL_FMT_ALL_LEFT,
    I2S_CHANNEL_FMT_ONLY_RIGHT,
    I2S_CHANNEL_FMT_ONLY_LEFT,
} i2s_channel_fmt_t;

typedef enum {
    I2S_COMM_FORMAT_STAND_I2S = 0x01,
    I2S_COMM_FORMAT_STAND_MSB = 0x02,
} i2s_comm_format_t;

#define ESP_INTR_FLAG_LEVEL1 (1 << 1)
#define I2S_PIN_NO_CHANGE (-1)

typedef struct {
    int mode; // i2s_mode_t flags
    uint32_t sample_rate;
    i2s_bits_per_sample_t bits_per_sample;
    i2s_channel_fmt_t channel_format;
    i2s_comm_format_t communication_format;
    int intr_alloc_flags;
    int dma_buf_count;
    int dma_buf_len;
    bool use_apll;
    bool tx_desc_auto_clear;
    int fixed_mclk;
} i2s_config_t;

typedef struct {
    int bck_io_num;
    int ws_io_num;
    int data_out_num;
    int data_in_num;
} i2s_pin_config_t;

// --- test controls ---
extern int32_t g_chan_amp[2][2]; // raw 32-bit slot value per [port][slot]
extern bool g_bus_fail[2];       // make driver install fail for a port

typedef unsigned TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))

inline esp_err_t i2s_driver_install(i2s_port_t port, const i2s_config_t *, int, void *)
{
    return g_bus_fail[port] ? ESP_FAIL : ESP_OK;
}
inline esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t *) { return ESP_OK; }
inline esp_err_t i2s_zero_dma_buffer(i2s_port_t) { return ESP_OK; }
inline esp_err_t i2s_stop(i2s_port_t) { return ESP_OK; }
inline esp_err_t i2s_driver_uninstall(i2s_port_t) { return ESP_OK; }

inline esp_err_t i2s_read(i2s_port_t port, void *dest, size_t size, size_t *bytes_read, TickType_t)
{
    int32_t *buf = (int32_t *) dest;
    size_t frames = size / (2 * sizeof(int32_t));
    for (size_t i = 0; i < frames; i++) {
        buf[2 * i + 0] = g_chan_amp[port][0];
        buf[2 * i + 1] = g_chan_amp[port][1];
    }
    *bytes_read = frames * 2 * sizeof(int32_t);
    return ESP_OK;
}

#endif
