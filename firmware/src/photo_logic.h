// Pure photo-upload pacing logic - no hardware includes, host-testable.
#ifndef PHOTO_LOGIC_H
#define PHOTO_LOGIC_H

// Should the main loop push one more photo chunk on this pass?
//
// Three conditions, in the order that matters:
//   1. there has to be a frame in flight at all;
//   2. audio outranks it - a queued audio packet ends the photo path for this
//      iteration, because audio is realtime and a photo can wait a few ms;
//   3. and no more than `limit` chunks leave per iteration, so a large frame
//      cannot monopolise the loop.
//
// `chunks_sent` counts chunks sent during THIS iteration and must start at
// zero each pass. The bug this replaces kept it in a static: the yield branch
// zeroed the count and then sent the chunk anyway, so a busy audio path made
// photos more aggressive rather than less - the limit could never accumulate
// while audio kept clearing it. With audio idle it misbehaved the other way,
// sending on two passes in three, because the count was only cleared on a
// pass it had already blocked.
static inline bool photo_should_send_chunk(bool uploading, bool have_frame, bool audio_subscribed,
                                           bool audio_pending, int chunks_sent, int limit)
{
    if (!uploading || !have_frame) {
        return false;
    }
    if (audio_subscribed && audio_pending) {
        return false;
    }
    return chunks_sent < limit;
}

#endif // PHOTO_LOGIC_H
