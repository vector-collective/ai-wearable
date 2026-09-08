// Pure event ring and CSV formatting - no hardware includes, host-testable.
#ifndef EVENT_RING_H
#define EVENT_RING_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// The timeline spine. Every event carries the boot's monotonic millis and
// the wall time if known; the pipeline resolves the rest from sync anchors.
// See docs/SPEC.md section 3.3.
typedef enum {
    EV_BOOT = 1,
    EV_SYNC,
    EV_SESSION_START,
    EV_SESSION_STOP,
    EV_BURST_START,
    EV_BURST_FRAME,
    EV_BURST_END,
    EV_QUIET_ON,
    EV_QUIET_OFF,
    EV_CANDIDATE, // device-tier new-voice candidate armed a hold-off
    EV_THERMAL,   // thermal state changed (thermal.h)
} event_type_t;

#define EVENT_DETAIL_LEN 48

typedef struct {
    uint32_t millis;
    uint64_t epoch_ms;
    uint8_t type;
    char detail[EVENT_DETAIL_LEN];
} event_t;

// Events raised before the card is mounted wait here; the recorder task
// drains it every pass once the card is up. When full, the OLDEST entry
// goes - a sync anchor arriving late is worth more than a stale frame
// record - and the drop is counted so the pipeline can see the gap.
#define EVENT_RING_SIZE 64

typedef struct {
    event_t buf[EVENT_RING_SIZE];
    uint8_t head; // next write
    uint8_t tail; // next read
    uint8_t count;
    uint16_t dropped;
} event_ring_t;

static inline void event_ring_init(event_ring_t *r)
{
    memset(r, 0, sizeof(*r));
}

static inline void event_ring_push(event_ring_t *r, const event_t *e)
{
    if (r->count == EVENT_RING_SIZE) {
        r->tail = (uint8_t) ((r->tail + 1) % EVENT_RING_SIZE);
        r->count--;
        r->dropped++;
    }
    r->buf[r->head] = *e;
    r->head = (uint8_t) ((r->head + 1) % EVENT_RING_SIZE);
    r->count++;
}

static inline bool event_ring_pop(event_ring_t *r, event_t *out)
{
    if (r->count == 0) {
        return false;
    }
    *out = r->buf[r->tail];
    r->tail = (uint8_t) ((r->tail + 1) % EVENT_RING_SIZE);
    r->count--;
    return true;
}

static inline const char *event_type_name(uint8_t t)
{
    switch (t) {
    case EV_BOOT:          return "boot";
    case EV_SYNC:          return "sync";
    case EV_SESSION_START: return "session_start";
    case EV_SESSION_STOP:  return "session_stop";
    case EV_BURST_START:   return "burst_start";
    case EV_BURST_FRAME:   return "burst_frame";
    case EV_BURST_END:     return "burst_end";
    case EV_QUIET_ON:      return "quiet_on";
    case EV_QUIET_OFF:     return "quiet_off";
    case EV_CANDIDATE:     return "candidate";
    case EV_THERMAL:       return "thermal";
    default:               return "unknown";
    }
}

// boot_id,millis,epoch_ms,type,detail\n  - epoch_ms is 0 when unsynced.
// Detail is written as-is; producers keep it free of commas and newlines.
static inline int event_format_csv(char *out, size_t n, uint32_t boot_id, const event_t *e)
{
    return snprintf(out, n, "%lu,%lu,%llu,%s,%s\n", (unsigned long) boot_id, (unsigned long) e->millis,
                    (unsigned long long) e->epoch_ms, event_type_name(e->type), e->detail);
}

#endif // EVENT_RING_H
