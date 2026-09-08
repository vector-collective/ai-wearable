// Pure decision logic for the device-tier voice path - no hardware includes,
// host-testable. Consumes one voice_feat_t per 100 ms block.
//
//   voice_gate     who is talking: SILENCE / OWN / OTHER
//   novelty        is this OTHER voice one we have heard lately
//
// The gate is level-first. The wearer's mouth is a fixed ~25 cm above the
// pendant and every other speaker is at least twice as far, so the wearer is
// at least 6 dB louder than anyone else at the same effort, and usually 12.
// A spectral tilt veto is available (own voice reaches the chest off-axis
// and through the body, so it is darker than a partner facing the mic) but
// it depends on the enclosure's acoustics, so it ships disabled and is set
// on the bench from the VOICE: serial line. See firmware/README.md.
//
// The novelty detector is deliberately coarse: a five-band long-term
// spectrum over ~1 s of speech, compared against a four-entry gallery of
// voices heard in the last few minutes. It cannot tell two similar voices
// apart and does not try to. It is the fallback tier (docs/SPEC.md 2.1): a
// candidate only arms a hold-off, a remote verdict overrides it, and the
// burst policy's gap and daily cap bound the cost of a wrong guess.
#ifndef VOICE_LOGIC_H
#define VOICE_LOGIC_H

#include <stdint.h>
#include <string.h>

#include "voice_dsp.h"

// ---------------------------------------------------------------------------
// Own-voice gate
// ---------------------------------------------------------------------------
enum { VOICE_SILENCE = 0, VOICE_OWN = 1, VOICE_OTHER = 2 };

typedef struct {
    float speech_ratio;     // speech when level > floor * speech_ratio ...
    float speech_min_level; // ... and above this absolute level
    float own_level;        // OWN when level > this ...
    float own_tilt_min_db;  // ... and smoothed tilt >= this. -1e9 disables.
    uint32_t hangover_ms;   // a state outlives its last qualifying block by this
    float floor_rise;       // per-block multiplier while level > floor (slow)
    float floor_fall;       // fraction of the gap closed per block while level < floor (fast)
} voice_gate_cfg_t;

typedef struct {
    float floor;         // tracked noise floor, level units
    float tilt_avg;      // smoothed tilt over speech blocks
    int state;           // VOICE_*
    bool speech;         // this block carried speech
    uint32_t hang_until; // ms
} voice_gate_t;

static inline void voice_gate_init(voice_gate_t *g)
{
    memset(g, 0, sizeof(*g));
    g->floor = 1.0f;
    g->state = VOICE_SILENCE;
}

static inline int voice_gate_step(voice_gate_t *g, const voice_gate_cfg_t *c, uint32_t now, const voice_feat_t *f)
{
    // Noise floor: minimum-statistics style. Falls quickly towards any
    // quieter block, climbs slowly while everything is louder, so a long
    // utterance cannot pull it up into the speech band but a pause resets it.
    if (f->level < g->floor) {
        g->floor += (f->level - g->floor) * c->floor_fall;
    } else {
        g->floor *= c->floor_rise;
    }
    if (g->floor < 1.0f) {
        g->floor = 1.0f;
    }

    float thresh = g->floor * c->speech_ratio;
    if (thresh < c->speech_min_level) {
        thresh = c->speech_min_level;
    }
    g->speech = f->level > thresh;

    bool hang_live = (int32_t) (now - g->hang_until) < 0;

    if (g->speech) {
        g->tilt_avg += (f->tilt_db - g->tilt_avg) * 0.3f;
        bool own_now = f->level > c->own_level && g->tilt_avg >= c->own_tilt_min_db;
        if (own_now) {
            g->state = VOICE_OWN;
            g->hang_until = now + c->hangover_ms;
        } else if (g->state == VOICE_OWN && hang_live) {
            // a soft syllable inside the wearer's own utterance: still OWN
        } else {
            g->state = VOICE_OTHER;
            g->hang_until = now + c->hangover_ms;
        }
    } else if (!hang_live) {
        g->state = VOICE_SILENCE;
    }
    return g->state;
}

// ---------------------------------------------------------------------------
// Novelty: have we heard this OTHER voice lately?
// ---------------------------------------------------------------------------
#define NOVELTY_GALLERY 4

typedef struct {
    uint16_t min_blocks;     // OTHER speech blocks before a segment is scored
    uint32_t segment_gap_ms; // this long without OTHER speech starts a new segment
    float dist_db;           // RMS band distance above which a voice is new
    uint32_t forget_ms;      // gallery entries unheard for this long stop counting
} novelty_cfg_t;

typedef struct {
    float prof[NOVELTY_GALLERY][VOICE_BANDS];
    uint32_t last_seen[NOVELTY_GALLERY];
    bool valid[NOVELTY_GALLERY];

    float seg[VOICE_BANDS]; // running mean profile of the current segment
    uint16_t seg_n;
    bool seg_scored;
    uint32_t last_other_ms;
    bool have_other;

    float last_dist; // distance that produced the last score, for logging
    int last_match;  // gallery slot matched or inserted by the last score
} novelty_t;

static inline void novelty_init(novelty_t *n)
{
    memset(n, 0, sizeof(*n));
    n->last_match = -1;
}

// Returns true when this block completes a segment that matches nothing in
// the gallery: a candidate new voice. The caller decides what to do with it.
static inline bool novelty_step(novelty_t *n, const novelty_cfg_t *c, uint32_t now, int state, bool speech,
                                const voice_feat_t *f)
{
    if (state != VOICE_OTHER || !speech) {
        return false;
    }
    if (n->have_other && (uint32_t) (now - n->last_other_ms) > c->segment_gap_ms) {
        n->seg_n = 0;
        n->seg_scored = false;
    }
    n->last_other_ms = now;
    n->have_other = true;

    float p[VOICE_BANDS];
    voice_profile(f, p);
    n->seg_n++;
    for (int k = 0; k < VOICE_BANDS; k++) {
        n->seg[k] += (p[k] - n->seg[k]) / (float) n->seg_n;
    }
    if (n->seg_scored || n->seg_n < c->min_blocks) {
        return false;
    }
    n->seg_scored = true;

    // Nearest live gallery entry; also note a free slot and the least
    // recently heard live one, for insertion below.
    int best = -1;
    float best_d = 0.0f;
    int free_slot = -1;
    int oldest = -1;
    for (int i = 0; i < NOVELTY_GALLERY; i++) {
        if (n->valid[i] && (uint32_t) (now - n->last_seen[i]) > c->forget_ms) {
            n->valid[i] = false;
        }
        if (!n->valid[i]) {
            if (free_slot < 0) {
                free_slot = i;
            }
            continue;
        }
        float d = voice_profile_dist(n->seg, n->prof[i]);
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
        if (oldest < 0 || (int32_t) (n->last_seen[i] - n->last_seen[oldest]) < 0) {
            oldest = i;
        }
    }

    if (best >= 0 && best_d <= c->dist_db) {
        // Known: fold the segment in and refresh.
        for (int k = 0; k < VOICE_BANDS; k++) {
            n->prof[best][k] += (n->seg[k] - n->prof[best][k]) * 0.3f;
        }
        n->last_seen[best] = now;
        n->last_dist = best_d;
        n->last_match = best;
        return false;
    }

    // New: insert, evicting the least recently heard if full. (oldest is
    // always set when free_slot is not: a full gallery has four live entries.)
    int slot = (free_slot >= 0) ? free_slot : oldest;
    memcpy(n->prof[slot], n->seg, sizeof(n->seg));
    n->last_seen[slot] = now;
    n->valid[slot] = true;
    n->last_dist = (best >= 0) ? best_d : -1.0f; // -1: gallery was empty
    n->last_match = slot;
    return true;
}

static inline int novelty_live_count(const novelty_t *n, const novelty_cfg_t *c, uint32_t now)
{
    int live = 0;
    for (int i = 0; i < NOVELTY_GALLERY; i++) {
        if (n->valid[i] && (uint32_t) (now - n->last_seen[i]) <= c->forget_ms) {
            live++;
        }
    }
    return live;
}

#endif // VOICE_LOGIC_H
