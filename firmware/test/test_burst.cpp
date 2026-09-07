// Host-side tests for the burst policy, time sync mapping, and event ring.
#include "burst_logic.h"
#include "event_ring.h"
#include "timesync.h"

#include <cassert>
#include <cstdio>
#include <cstring>

// Small numbers so a test reads in one line: suppress 1000, gap 100,
// hold-off 50, day 10000, cap 3.
static const burst_cfg_t C = {1000, 100, 50, 10000, 3};

static burst_policy_t fresh()
{
    burst_policy_t p;
    burst_policy_init(&p);
    return p;
}

static void test_timesync()
{
    timesync_t t;
    timesync_init(&t);
    assert(timesync_now(&t, 12345) == 0);
    timesync_set(&t, 1700000000000ULL, 5000);
    assert(timesync_now(&t, 5000) == 1700000000000ULL);
    assert(timesync_now(&t, 6500) == 1700000001500ULL);
    // millis() wraps at 2^32; a sync just before and a query just after
    // must still give the right delta
    timesync_set(&t, 1700000000000ULL, 0xFFFFFF00u);
    assert(timesync_now(&t, 0x00000100u) == 1700000000000ULL + 0x200);
    printf("timesync map + wrap OK\n");
}

static void test_event_ring()
{
    event_ring_t r;
    event_ring_init(&r);
    event_t e = {};
    for (int i = 0; i < 5; i++) {
        e.millis = (uint32_t) i;
        e.type = EV_BURST_FRAME;
        snprintf(e.detail, sizeof(e.detail), "f%d", i);
        event_ring_push(&r, &e);
    }
    event_t o;
    assert(event_ring_pop(&r, &o) && o.millis == 0 && strcmp(o.detail, "f0") == 0);
    assert(event_ring_pop(&r, &o) && o.millis == 1);
    assert(r.count == 3);
    // overflow drops the OLDEST and counts it
    for (int i = 5; i < 5 + EVENT_RING_SIZE; i++) {
        e.millis = (uint32_t) i;
        event_ring_push(&r, &e);
    }
    assert(r.count == EVENT_RING_SIZE);
    assert(r.dropped == 3);
    assert(event_ring_pop(&r, &o) && o.millis == 5); // 2,3,4 were dropped
    // csv line
    event_t s = {};
    s.millis = 4242;
    s.epoch_ms = 1700000004242ULL;
    s.type = EV_SYNC;
    strcpy(s.detail, "millis=4242 epoch=1700000004242");
    char line[128];
    event_format_csv(line, sizeof(line), 7, &s);
    assert(strcmp(line, "7,4242,1700000004242,sync,millis=4242 epoch=1700000004242\n") == 0);
    // unsynced rows carry 0
    s.epoch_ms = 0;
    s.type = EV_BOOT;
    strcpy(s.detail, "fw=x");
    event_format_csv(line, sizeof(line), 7, &s);
    assert(strcmp(line, "7,4242,0,boot,fw=x\n") == 0);
    printf("event ring FIFO, overflow, csv OK\n");
}

static void test_remote_verdicts()
{
    burst_policy_t p = fresh();
    uint32_t h;
    // new voice from the server: fires
    assert(burst_policy_remote(&p, &C, 1000, 77, true));
    // same hint again inside suppress: blocked
    assert(!burst_policy_remote(&p, &C, 1500, 77, true));
    assert(burst_policy_blocked(&p, &C, 1500, 77) == BURST_BLOCK_SUPPRESSED);
    // different hint, but inside min_gap: blocked by the gap
    assert(burst_policy_blocked(&p, &C, 1050, 88) == BURST_BLOCK_GAP);
    // different hint after the gap: fires
    assert(burst_policy_remote(&p, &C, 1200, 88, true));
    // known voice: never fires, and clears an armed hold-off
    assert(burst_policy_local_candidate(&p, &C, 1400, 99));
    assert(p.armed);
    assert(!burst_policy_remote(&p, &C, 1410, 99, false));
    assert(!p.armed);
    assert(!burst_policy_tick(&p, &C, 1500, &h));
    printf("remote verdicts: new fires, known cancels, suppression + gap OK\n");
}

static void test_holdoff()
{
    burst_policy_t p = fresh();
    uint32_t h = 0;
    // local candidate arms; nothing fires before the hold-off
    assert(burst_policy_local_candidate(&p, &C, 2000, 5));
    assert(!burst_policy_tick(&p, &C, 2040, &h));
    // a second candidate while armed is ignored
    assert(!burst_policy_local_candidate(&p, &C, 2041, 6));
    // hold-off expires: fires on the device's authority with the FIRST hint
    assert(burst_policy_tick(&p, &C, 2050, &h));
    assert(h == 5);
    assert(!p.armed);
    // and only once
    assert(!burst_policy_tick(&p, &C, 2060, &h));
    printf("hold-off: arms, waits, fires once on expiry OK\n");

    // a remote NEW inside the hold-off fires immediately and the later tick
    // does not fire a second burst
    p = fresh();
    assert(burst_policy_local_candidate(&p, &C, 3000, 5));
    assert(burst_policy_remote(&p, &C, 3020, 5, true));
    assert(!burst_policy_tick(&p, &C, 3100, &h));
    printf("hold-off: remote NEW wins inside the window OK\n");

    // quiet mode arriving during a hold-off disarms it
    p = fresh();
    assert(burst_policy_local_candidate(&p, &C, 4000, 5));
    burst_policy_set_quiet(&p, true);
    assert(!p.armed);
    assert(!burst_policy_tick(&p, &C, 4100, &h));
    printf("hold-off: quiet disarms OK\n");
}

static void test_cap_and_quiet()
{
    burst_policy_t p = fresh();
    uint32_t t = 10000;
    // cap 3: three distinct speakers spaced past the gap
    assert(burst_policy_remote(&p, &C, t, 1, true));
    assert(burst_policy_remote(&p, &C, t + 200, 2, true));
    assert(burst_policy_remote(&p, &C, t + 400, 3, true));
    assert(burst_policy_blocked(&p, &C, t + 600, 4) == BURST_BLOCK_CAP);
    assert(!burst_policy_remote(&p, &C, t + 600, 4, true));
    // the day rolls over: cap resets
    assert(burst_policy_remote(&p, &C, t + 10000, 4, true));
    printf("daily cap + rollover OK\n");

    // quiet blocks verdicts, local, and force alike
    p = fresh();
    burst_policy_set_quiet(&p, true);
    assert(!burst_policy_remote(&p, &C, 100, 1, true));
    assert(!burst_policy_local_candidate(&p, &C, 100, 1));
    assert(!burst_policy_force(&p, 100));
    burst_policy_set_quiet(&p, false);
    assert(burst_policy_force(&p, 200));
    printf("quiet mode OK\n");

    // force ignores the cap and suppression
    p = fresh();
    assert(burst_policy_remote(&p, &C, 100, 9, true));
    assert(burst_policy_force(&p, 110)); // inside the gap, still fires
    printf("force bypasses cap/gap OK\n");

    // hint 0 (device tier, no identity): no suppression key, gap still applies
    p = fresh();
    assert(burst_policy_remote(&p, &C, 100, 0, true));
    assert(burst_policy_blocked(&p, &C, 150, 0) == BURST_BLOCK_GAP);
    assert(burst_policy_blocked(&p, &C, 250, 0) == BURST_OK);
    printf("hint 0 OK\n");
}

static void test_wrap()
{
    // policy timestamps straddling the uint32 wrap
    burst_policy_t p = fresh();
    uint32_t t = 0xFFFFFFF0u;
    assert(burst_policy_remote(&p, &C, t, 1, true));
    // 50ms later (post-wrap) the gap still holds
    assert(burst_policy_blocked(&p, &C, t + 50, 2) == BURST_BLOCK_GAP);
    // 150ms later it is clear
    assert(burst_policy_blocked(&p, &C, t + 150, 2) == BURST_OK);
    // and the suppressed hint stays suppressed across the wrap
    assert(burst_policy_blocked(&p, &C, t + 500, 1) == BURST_BLOCK_SUPPRESSED);
    printf("uint32 wrap OK\n");
}

int main()
{
    test_timesync();
    test_event_ring();
    test_remote_verdicts();
    test_holdoff();
    test_cap_and_quiet();
    test_wrap();
    printf("ALL BURST TESTS PASSED\n");
    return 0;
}
