// Pure wall-clock mapping - no hardware includes, host-testable.
#ifndef TIMESYNC_H
#define TIMESYNC_H

#include <stdint.h>

// There is no RTC. The phone writes Unix epoch milliseconds on every connect
// and the device remembers what millis() read at that instant; wall time is
// then epoch_at_sync + (millis() - millis_at_sync). ESP32 drift is tens of
// ppm - under a second across a session - and every reconnect re-anchors.
//
// The subtraction is done in uint32 so it stays correct across the 49.7-day
// millis() wrap, provided the gap between sync and query is under 49 days.
// A pendant that has not seen its phone for 49 days has bigger problems.
typedef struct {
    uint64_t epoch_ms_at_sync;
    uint32_t millis_at_sync;
    bool synced;
} timesync_t;

static inline void timesync_init(timesync_t *t)
{
    t->epoch_ms_at_sync = 0;
    t->millis_at_sync = 0;
    t->synced = false;
}

static inline void timesync_set(timesync_t *t, uint64_t epoch_ms, uint32_t now_ms)
{
    t->epoch_ms_at_sync = epoch_ms;
    t->millis_at_sync = now_ms;
    t->synced = true;
}

// 0 when unsynced. Callers log the raw millis alongside, so a 0 here is a
// gap the pipeline fills from the next sync anchor, not lost data.
static inline uint64_t timesync_now(const timesync_t *t, uint32_t now_ms)
{
    if (!t->synced) {
        return 0;
    }
    return t->epoch_ms_at_sync + (uint64_t) (uint32_t) (now_ms - t->millis_at_sync);
}

#endif // TIMESYNC_H
