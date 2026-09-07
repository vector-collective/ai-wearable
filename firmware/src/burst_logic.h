// Pure burst policy - no hardware includes, host-testable.
#ifndef BURST_LOGIC_H
#define BURST_LOGIC_H

#include <stdint.h>
#include <string.h>

// Whether a new-voice photo burst should fire, and when. See docs/SPEC.md
// sections 2.1 and 2.2 for the reasoning; this file is the mechanism.
//
// Two ways a burst is proposed:
//   remote  - the phone or server has diarized the audio and says "new
//             voice" (fire) or "known voice" (stand down). Accurate, late.
//   local   - the device's own detector sees a candidate. Fast, crude.
//
// The device does not measure network latency. On a local candidate it ARMS
// a hold-off; a remote verdict inside the hold-off wins, silence means the
// device fires on its own authority when the hold-off expires. A slow
// server, a dead phone and a disconnected link all degrade to the same path
// without any of them having to be detected.
//
// Everything is gated by suppression, because diarization clusters wobble
// and the same person would otherwise trigger repeatedly.

typedef enum { BURST_SRC_DEVICE = 0, BURST_SRC_PHONE = 1, BURST_SRC_SERVER = 2 } burst_src_t;

typedef struct {
    uint32_t suppress_ms; // per-hint: once fired for a hint, ignore it this long
    uint32_t min_gap_ms;  // between ANY two bursts - the device tier has no hint
    uint32_t holdoff_ms;  // how long a local candidate waits for a remote verdict
    uint32_t day_ms;      // rolling window for the cap
    uint16_t daily_cap;
} burst_cfg_t;

#define BURST_SUPPRESS_SLOTS 16

typedef struct {
    // recent bursts, by hint, for per-speaker suppression
    uint32_t sup_hint[BURST_SUPPRESS_SLOTS];
    uint32_t sup_at[BURST_SUPPRESS_SLOTS];
    uint8_t sup_next;
    // any-burst gap
    bool any_fired;
    uint32_t last_fire_ms;
    // rolling daily cap
    bool day_open;
    uint32_t day_start_ms;
    uint16_t day_count;
    // hold-off
    bool armed;
    uint32_t armed_at;
    uint32_t armed_hint;
    bool quiet;
} burst_policy_t;

static inline void burst_policy_init(burst_policy_t *p)
{
    memset(p, 0, sizeof(*p));
}

static inline uint32_t burst_elapsed(uint32_t now, uint32_t then)
{
    return (uint32_t) (now - then); // wrap-safe
}

// Why a burst may not fire right now, or 0 if it may. Hint 0 means "no
// speaker identity" - per-hint suppression is skipped, min_gap still holds.
enum { BURST_OK = 0, BURST_BLOCK_QUIET, BURST_BLOCK_CAP, BURST_BLOCK_SUPPRESSED, BURST_BLOCK_GAP };

static inline int burst_policy_blocked(burst_policy_t *p, const burst_cfg_t *c, uint32_t now, uint32_t hint)
{
    if (p->quiet) {
        return BURST_BLOCK_QUIET;
    }
    if (p->day_open && burst_elapsed(now, p->day_start_ms) >= c->day_ms) {
        p->day_open = false;
        p->day_count = 0;
    }
    if (p->day_open && p->day_count >= c->daily_cap) {
        return BURST_BLOCK_CAP;
    }
    if (p->any_fired && burst_elapsed(now, p->last_fire_ms) < c->min_gap_ms) {
        return BURST_BLOCK_GAP;
    }
    if (hint != 0) {
        for (int i = 0; i < BURST_SUPPRESS_SLOTS; i++) {
            if (p->sup_hint[i] == hint && burst_elapsed(now, p->sup_at[i]) < c->suppress_ms) {
                return BURST_BLOCK_SUPPRESSED;
            }
        }
    }
    return BURST_OK;
}

static inline void burst_policy_record(burst_policy_t *p, uint32_t now, uint32_t hint)
{
    if (!p->day_open) {
        p->day_open = true;
        p->day_start_ms = now;
        p->day_count = 0;
    }
    p->day_count++;
    p->any_fired = true;
    p->last_fire_ms = now;
    if (hint != 0) {
        p->sup_hint[p->sup_next] = hint;
        p->sup_at[p->sup_next] = now;
        p->sup_next = (uint8_t) ((p->sup_next + 1) % BURST_SUPPRESS_SLOTS);
    }
    p->armed = false;
}

// A remote verdict. Cancels any hold-off either way; fires only on "new".
// Returns true if a burst should start now.
static inline bool burst_policy_remote(burst_policy_t *p, const burst_cfg_t *c, uint32_t now, uint32_t hint,
                                       bool is_new)
{
    p->armed = false;
    if (!is_new) {
        return false;
    }
    if (burst_policy_blocked(p, c, now, hint) != BURST_OK) {
        return false;
    }
    burst_policy_record(p, now, hint);
    return true;
}

// The device's own detector saw a candidate. Arms the hold-off unless the
// policy would refuse the burst anyway, in which case there is nothing to
// wait for. Returns true if newly armed.
static inline bool burst_policy_local_candidate(burst_policy_t *p, const burst_cfg_t *c, uint32_t now,
                                                uint32_t hint)
{
    if (p->armed) {
        return false;
    }
    if (burst_policy_blocked(p, c, now, hint) != BURST_OK) {
        return false;
    }
    p->armed = true;
    p->armed_at = now;
    p->armed_hint = hint;
    return true;
}

// Called every loop pass. Returns true when an expired hold-off fires on
// the device's own authority; *hint_out gets the candidate's hint.
static inline bool burst_policy_tick(burst_policy_t *p, const burst_cfg_t *c, uint32_t now, uint32_t *hint_out)
{
    if (!p->armed) {
        return false;
    }
    if (burst_elapsed(now, p->armed_at) < c->holdoff_ms) {
        return false;
    }
    uint32_t hint = p->armed_hint;
    p->armed = false;
    if (burst_policy_blocked(p, c, now, hint) != BURST_OK) {
        return false; // quiet mode or the cap arrived during the hold-off
    }
    burst_policy_record(p, now, hint);
    if (hint_out) {
        *hint_out = hint;
    }
    return true;
}

// Manual burst from the phone. Quiet mode still wins; nothing else does.
static inline bool burst_policy_force(burst_policy_t *p, uint32_t now)
{
    if (p->quiet) {
        return false;
    }
    burst_policy_record(p, now, 0);
    return true;
}

static inline void burst_policy_set_quiet(burst_policy_t *p, bool quiet)
{
    p->quiet = quiet;
    if (quiet) {
        p->armed = false;
    }
}

#endif // BURST_LOGIC_H
