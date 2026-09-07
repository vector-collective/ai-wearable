#include "events.h"

#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <stdarg.h>

#include "config.h"
#include "timesync.h"

static event_ring_t ring;
static timesync_t ts;
static uint32_t boot_id = 0;
static portMUX_TYPE ring_mux = portMUX_INITIALIZER_UNLOCKED;

void events_init()
{
    event_ring_init(&ring);
    timesync_init(&ts);

    // boot_id is what makes millis() meaningful across power cycles: every
    // event carries (boot_id, millis) and the pipeline reconciles per boot.
    Preferences prefs;
    if (prefs.begin("pendant", false)) {
        boot_id = prefs.getUInt("boot", 0) + 1;
        prefs.putUInt("boot", boot_id);
        prefs.end();
    } else {
        // NVS unavailable: still produce a value that will not collide with
        // a real counter, so the timeline stays parseable.
        boot_id = 0x80000000u | (uint32_t) (esp_random() & 0x7FFFFFFFu);
    }
    events_logf(EV_BOOT, "fw=%s", FIRMWARE_VERSION_STRING);
    Serial.printf("EVENTS: boot %lu\n", (unsigned long) boot_id);
}

uint32_t events_boot_id()
{
    return boot_id;
}

void events_time_sync(uint64_t epoch_ms)
{
    uint32_t now = millis();
    timesync_set(&ts, epoch_ms, now);
    // The sync event itself is the anchor the pipeline interpolates from, so
    // its detail carries the raw pair even though epoch_ms is also in the
    // row: a later parser must not have to trust the row's epoch column to
    // rebuild the mapping.
    events_logf(EV_SYNC, "millis=%lu epoch=%llu", (unsigned long) now, (unsigned long long) epoch_ms);
    Serial.printf("EVENTS: time sync %llu at millis %lu\n", (unsigned long long) epoch_ms, (unsigned long) now);
}

bool events_time_synced()
{
    return ts.synced;
}

uint64_t events_epoch_now()
{
    return timesync_now(&ts, millis());
}

void events_log(uint8_t type, const char *detail)
{
    event_t e;
    e.millis = millis();
    e.epoch_ms = timesync_now(&ts, e.millis);
    e.type = type;
    if (detail) {
        strncpy(e.detail, detail, EVENT_DETAIL_LEN - 1);
        e.detail[EVENT_DETAIL_LEN - 1] = '\0';
    } else {
        e.detail[0] = '\0';
    }
    portENTER_CRITICAL(&ring_mux);
    event_ring_push(&ring, &e);
    portEXIT_CRITICAL(&ring_mux);
}

void events_logf(uint8_t type, const char *fmt, ...)
{
    char buf[EVENT_DETAIL_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    events_log(type, buf);
}

int events_flush_to_sd()
{
    portENTER_CRITICAL(&ring_mux);
    uint8_t pending = ring.count;
    portEXIT_CRITICAL(&ring_mux);
    if (pending == 0) {
        return 0;
    }
    // Open before popping: a failed open then costs nothing and the ring
    // keeps its order for the next pass.
    File f = SD.open(EVENTS_PATH, FILE_APPEND);
    if (!f) {
        return 0;
    }
    int written = 0;
    for (;;) {
        event_t e;
        portENTER_CRITICAL(&ring_mux);
        bool have = event_ring_pop(&ring, &e);
        uint16_t dropped = ring.dropped;
        ring.dropped = 0;
        portEXIT_CRITICAL(&ring_mux);
        if (!have) {
            break;
        }
        if (dropped) {
            char line[64];
            snprintf(line, sizeof(line), "%lu,%lu,0,dropped,%u\n", (unsigned long) boot_id,
                     (unsigned long) e.millis, (unsigned) dropped);
            f.print(line);
        }
        char line[EVENT_DETAIL_LEN + 64];
        event_format_csv(line, sizeof(line), boot_id, &e);
        f.print(line);
        written++;
    }
    f.close();
    return written;
}
