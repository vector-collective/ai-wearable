// Host-side tests for photo upload pacing.
#include "photo_logic.h"

#include <cassert>
#include <cstdio>

#define LIMIT 2

// Count the chunks a single main-loop pass would send, given a frame big
// enough that it never runs out mid-pass.
static int chunks_in_one_pass(bool audio_subscribed, bool audio_pending)
{
    int sent = 0;
    while (photo_should_send_chunk(true, true, audio_subscribed, audio_pending, sent, LIMIT)) {
        sent++;
    }
    return sent;
}

// The defect this replaces, transcribed: one static counter across passes, the
// yield branch clearing it and then sending anyway.
static void old_behaviour(int passes, bool audio_subscribed, bool audio_pending, int *sent_out,
                          int *blocked_out)
{
    static int chunks_this_loop;
    chunks_this_loop = 0;
    int sent = 0, blocked = 0;
    for (int i = 0; i < passes; i++) {
        if (chunks_this_loop < LIMIT) {
            if (audio_subscribed && audio_pending) {
                chunks_this_loop = 0; // "yield"...
            } else {
                chunks_this_loop++;
            }
            sent++; // ...and send regardless. This is the bug.
        } else {
            chunks_this_loop = 0;
            blocked++;
        }
    }
    *sent_out = sent;
    *blocked_out = blocked;
}

int main()
{
    // Nothing in flight.
    assert(!photo_should_send_chunk(false, true, false, false, 0, LIMIT));
    assert(!photo_should_send_chunk(true, false, false, false, 0, LIMIT));
    printf("idle -> no chunks OK\n");

    // Cap holds within a pass.
    assert(chunks_in_one_pass(false, false) == LIMIT);
    printf("cap per pass OK\n");

    // Audio waiting stops the photo path dead.
    assert(chunks_in_one_pass(true, true) == 0);
    // Subscribed but nothing queued is not a reason to stall.
    assert(chunks_in_one_pass(true, false) == LIMIT);
    // Unsubscribed: a stale ring-buffer difference must not gate photos.
    assert(chunks_in_one_pass(false, true) == LIMIT);
    printf("audio yield OK\n");

    // REGRESSION, audio busy: the old code sent on every pass because the
    // yield cleared the counter it was supposed to accumulate. The fix sends
    // nothing at all while audio is queued.
    int old_sent = 0, old_blocked = 0;
    old_behaviour(30, true, true, &old_sent, &old_blocked);
    assert(old_sent == 30 && old_blocked == 0);
    int now_sent = 0;
    for (int i = 0; i < 30; i++) {
        now_sent += chunks_in_one_pass(true, true);
    }
    assert(now_sent == 0);
    printf("regression, audio busy: was %d chunks over 30 passes, now %d OK\n", old_sent, now_sent);

    // REGRESSION, audio idle: the old code stuttered 2-on/1-off because the
    // counter was only cleared on a pass it had already blocked.
    old_behaviour(30, false, false, &old_sent, &old_blocked);
    assert(old_blocked == 10); // one pass in three wasted
    now_sent = 0;
    for (int i = 0; i < 30; i++) {
        now_sent += chunks_in_one_pass(false, false);
    }
    assert(now_sent == 30 * LIMIT);
    printf("regression, audio idle: was %d blocked passes in 30, now full %d chunks OK\n", old_blocked,
           now_sent);

    // A limit of zero disables the path rather than looping forever.
    assert(!photo_should_send_chunk(true, true, false, false, 0, 0));
    printf("zero limit OK\n");

    printf("ALL PHOTO TESTS PASSED\n");
    return 0;
}
