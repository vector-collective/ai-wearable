#include "burst.h"

#include <Arduino.h>

#include "app.h"
#include "burst_logic.h"
#include "config.h"
#include "events.h"
#include "sd_recorder.h"

static burst_policy_t policy;
static const burst_cfg_t cfg = {
    BURST_SUPPRESS_MS, BURST_MIN_GAP_MS, BURST_HOLDOFF_MS, BURST_DAY_MS, BURST_DAILY_CAP,
};

enum {
    OP_VERDICT_NEW = 0x01,
    OP_VERDICT_KNOWN = 0x02,
    OP_BURST_FORCE = 0x03,
    OP_QUIET_ON = 0x04,
    OP_QUIET_OFF = 0x05,
    OP_CANCEL = 0x06,
};

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t) (p[0] | (p[1] << 8));
}

static void start_burst(uint8_t count, uint16_t interval_ms, uint8_t src, uint32_t hint)
{
    app_register_activity();
    if (sd_recorder_burst(count, interval_ms, src, hint)) {
        Serial.printf("BURST: start src=%u hint=%lu count=%u interval=%u\n", src, (unsigned long) hint, count,
                      interval_ms);
    } else {
        Serial.println("BURST: recorder refused");
    }
}

void burst_init()
{
    burst_policy_init(&policy);
}

void burst_loop(uint32_t now)
{
    uint32_t hint = 0;
    if (burst_policy_tick(&policy, &cfg, now, &hint)) {
        start_burst(BURST_COUNT_DEFAULT, BURST_INTERVAL_MS_DEFAULT, BURST_SRC_DEVICE, hint);
    }
}

void burst_local_candidate(uint32_t hint)
{
    if (burst_policy_local_candidate(&policy, &cfg, millis(), hint)) {
        Serial.printf("BURST: local candidate hint=%lu, hold-off armed\n", (unsigned long) hint);
    }
}

bool burst_quiet()
{
    return policy.quiet;
}

void burst_handle_ctrl(const uint8_t *d, size_t n)
{
    if (n < 1) {
        return;
    }
    uint32_t now = millis();
    switch (d[0]) {
    case OP_VERDICT_NEW:
    case OP_VERDICT_KNOWN: {
        if (n < 6) {
            return;
        }
        uint8_t src = d[1];
        uint32_t hint = le32(d + 2);
        bool is_new = d[0] == OP_VERDICT_NEW;
        if (burst_policy_remote(&policy, &cfg, now, hint, is_new)) {
            start_burst(BURST_COUNT_DEFAULT, BURST_INTERVAL_MS_DEFAULT, src, hint);
        } else if (!is_new) {
            Serial.printf("BURST: known hint=%lu, stand down\n", (unsigned long) hint);
        } else {
            Serial.printf("BURST: new hint=%lu blocked (%d)\n", (unsigned long) hint,
                          burst_policy_blocked(&policy, &cfg, now, hint));
        }
        break;
    }
    case OP_BURST_FORCE: {
        if (n < 4) {
            return;
        }
        uint8_t count = d[1];
        uint16_t interval = le16(d + 2);
        if (count < 1 || count > 60 || interval < 500 || interval > 60000) {
            Serial.println("BURST: force out of range");
            return;
        }
        if (burst_policy_force(&policy, now)) {
            start_burst(count, interval, BURST_SRC_PHONE, 0);
        } else {
            Serial.println("BURST: force refused, quiet mode");
        }
        break;
    }
    case OP_QUIET_ON:
        burst_policy_set_quiet(&policy, true);
        sd_recorder_burst_cancel();
        events_log(EV_QUIET_ON, NULL);
        Serial.println("BURST: quiet on");
        break;
    case OP_QUIET_OFF:
        burst_policy_set_quiet(&policy, false);
        events_log(EV_QUIET_OFF, NULL);
        Serial.println("BURST: quiet off");
        break;
    case OP_CANCEL:
        sd_recorder_burst_cancel();
        break;
    default:
        Serial.printf("BURST: unknown op 0x%02x\n", d[0]);
        break;
    }
}
