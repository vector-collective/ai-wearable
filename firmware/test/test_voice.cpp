// Host-side tests for the mic-path DSP, the own-voice gate, the device-tier
// novelty detector, and the thermal policy. The gate and novelty tests use
// the shipped defaults from config.h so a bad default fails here first.
#include "config.h"
#include "thermal.h"
#include "voice_dsp.h"
#include "voice_logic.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#define FS 16000.0f
#define N MIC_BUFFER_SAMPLES

static const voice_gate_cfg_t GATE = {
    VOICE_SPEECH_RATIO, VOICE_SPEECH_MIN_LEVEL, VOICE_OWN_LEVEL, VOICE_OWN_TILT_MIN_DB,
    VOICE_HANGOVER_MS,  VOICE_FLOOR_RISE,       VOICE_FLOOR_FALL,
};
static const novelty_cfg_t NOV = {NOVELTY_MIN_BLOCKS, NOVELTY_SEGMENT_GAP_MS, NOVELTY_DIST_DB, NOVELTY_FORGET_MS};

static bool near(float v, float target, float tol_frac)
{
    float tol = fabsf(target) * tol_frac;
    return fabsf(v - target) <= tol;
}

// ---------------------------------------------------------------------------
// Synthetic signals
// ---------------------------------------------------------------------------
static float frand(uint32_t *s) // uniform [0,1)
{
    *s = *s * 1664525u + 1013904223u;
    return (float) (*s >> 8) / 16777216.0f;
}

// A harmonic "voice": partials at k*f0 up to 6 kHz with a fixed spectral
// slope, +-1 dB per-partial jitter per block, continuous phase across blocks.
// Each block is rescaled to an exact mean-abs level so the gate tests can
// reason in level units.
struct Voice {
    float f0;
    float tilt_db_oct;
    int nh;
    float phase[64];
    uint32_t seed;
};

static void voice_init(Voice *v, float f0, float tilt_db_oct, uint32_t seed)
{
    v->f0 = f0;
    v->tilt_db_oct = tilt_db_oct;
    v->nh = (int) (6000.0f / f0);
    if (v->nh > 63) {
        v->nh = 63;
    }
    memset(v->phase, 0, sizeof(v->phase));
    v->seed = seed;
}

static void voice_block(Voice *v, float level, int16_t *out)
{
    static float tmp[N];
    float amp[64];
    for (int k = 1; k <= v->nh; k++) {
        float slope = powf((float) k, -v->tilt_db_oct / 6.0206f);
        float jitter = powf(10.0f, (frand(&v->seed) * 2.0f - 1.0f) / 20.0f);
        amp[k] = slope * jitter;
    }
    float acc = 0.0f;
    for (int i = 0; i < N; i++) {
        float x = 0.0f;
        for (int k = 1; k <= v->nh; k++) {
            x += amp[k] * sinf(v->phase[k]);
            v->phase[k] += 2.0f * VOICE_PI * (float) k * v->f0 / FS;
            if (v->phase[k] > 2.0f * VOICE_PI) {
                v->phase[k] -= 2.0f * VOICE_PI;
            }
        }
        tmp[i] = x;
        acc += fabsf(x);
    }
    float scale = level / (acc / (float) N);
    for (int i = 0; i < N; i++) {
        float s = tmp[i] * scale;
        if (s > 32767.0f) s = 32767.0f;
        if (s < -32768.0f) s = -32768.0f;
        out[i] = (int16_t) s;
    }
}

static void noise_block(float level, int16_t *out, uint32_t *seed)
{
    // uniform [-1,1] has mean-abs 0.5
    for (int i = 0; i < N; i++) {
        out[i] = (int16_t) ((frand(seed) * 2.0f - 1.0f) * 2.0f * level);
    }
}

static void sine_block(float hz, float amp, float *phase, int16_t *out)
{
    for (int i = 0; i < N; i++) {
        out[i] = (int16_t) (amp * sinf(*phase));
        *phase += 2.0f * VOICE_PI * hz / FS;
        if (*phase > 2.0f * VOICE_PI) *phase -= 2.0f * VOICE_PI;
    }
}

// ---------------------------------------------------------------------------
// DSP
// ---------------------------------------------------------------------------
static float hp_gain_at(float hz)
{
    hp1_t f;
    hp1_init(&f, MIC_HIGHPASS_HZ, FS);
    float phase = 0.0f;
    float peak = 0.0f;
    int16_t blk[N];
    for (int b = 0; b < 30; b++) { // 3 s; measure the last second
        sine_block(hz, 1000.0f, &phase, blk);
        for (int i = 0; i < N; i++) {
            float y = hp1_step(&f, (float) blk[i]);
            if (b >= 20 && fabsf(y) > peak) peak = fabsf(y);
        }
    }
    return peak / 1000.0f;
}

static void test_hp()
{
    float g110 = hp_gain_at(110.0f);
    float g2k = hp_gain_at(2000.0f);
    float g30 = hp_gain_at(30.0f);
    printf("hp: |H| at 110 Hz = %.3f, 2 kHz = %.3f, 30 Hz = %.3f\n", g110, g2k, g30);
    assert(near(g110, 0.7071f, 0.03f));
    assert(g2k > 0.98f);
    assert(g30 < 0.30f);
    // DC dies
    hp1_t f;
    hp1_init(&f, MIC_HIGHPASS_HZ, FS);
    float y = 0.0f;
    for (int i = 0; i < 16000; i++) y = hp1_step(&f, 1000.0f);
    assert(fabsf(y) < 1.0f);
    // disabled = pass-through
    hp1_init(&f, 0.0f, FS);
    assert(hp1_step(&f, 123.0f) == 123.0f);
    printf("high-pass OK\n");
}

static int loudest_band(const voice_feat_t *f)
{
    int b = 0;
    for (int k = 1; k < VOICE_BANDS; k++) {
        if (f->band_db[k] > f->band_db[b]) b = k;
    }
    return b;
}

static void test_bands()
{
    voice_feat_state_t s;
    voice_feat_t f;
    int16_t blk[N];
    const float probe[3] = {250.0f, 1000.0f, 4000.0f};
    const int expect[3] = {0, 2, 4};
    for (int p = 0; p < 3; p++) {
        voice_feat_init(&s, FS);
        float phase = 0.0f;
        for (int b = 0; b < 3; b++) {
            sine_block(probe[p], 1000.0f, &phase, blk);
            voice_feat_block(&s, blk, N, &f);
        }
        printf("bands @%4.0f Hz: %5.1f %5.1f %5.1f %5.1f %5.1f  tilt %+5.1f  level %.0f\n", probe[p], f.band_db[0],
               f.band_db[1], f.band_db[2], f.band_db[3], f.band_db[4], f.tilt_db, f.level);
        assert(loudest_band(&f) == expect[p]);
        if (probe[p] < 2000.0f) {
            // mean |sin| = 2/pi; at 4 kHz there are only four samples per
            // cycle and the mean-abs of {0,1,0,-1} is 0.5, not 0.64
            assert(near(f.level, 1000.0f * 2.0f / VOICE_PI, 0.02f));
        }
    }
    // tilt sign follows the spectrum
    voice_feat_init(&s, FS);
    float ph = 0.0f;
    for (int b = 0; b < 3; b++) { sine_block(250.0f, 1000.0f, &ph, blk); voice_feat_block(&s, blk, N, &f); }
    assert(f.tilt_db > 20.0f);
    voice_feat_init(&s, FS);
    ph = 0.0f;
    for (int b = 0; b < 3; b++) { sine_block(4000.0f, 1000.0f, &ph, blk); voice_feat_block(&s, blk, N, &f); }
    assert(f.tilt_db < -20.0f);
    // profile removes level: same tone at two amplitudes, same profile
    float p1[VOICE_BANDS], p2[VOICE_BANDS];
    voice_feat_init(&s, FS);
    ph = 0.0f;
    for (int b = 0; b < 3; b++) { sine_block(1000.0f, 1000.0f, &ph, blk); voice_feat_block(&s, blk, N, &f); }
    voice_profile(&f, p1);
    voice_feat_init(&s, FS);
    ph = 0.0f;
    for (int b = 0; b < 3; b++) { sine_block(1000.0f, 100.0f, &ph, blk); voice_feat_block(&s, blk, N, &f); }
    voice_profile(&f, p2);
    printf("profile distance across a 20 dB level change: %.2f dB\n", voice_profile_dist(p1, p2));
    assert(voice_profile_dist(p1, p2) < 0.5f);
    // digital silence is finite
    voice_feat_init(&s, FS);
    memset(blk, 0, sizeof(blk));
    voice_feat_block(&s, blk, N, &f);
    assert(f.level == 0.0f && f.band_db[0] > -40.0f && f.band_db[0] < -20.0f);
    printf("filterbank OK\n");
}

// ---------------------------------------------------------------------------
// Gate, driven with hand-made features
// ---------------------------------------------------------------------------
static voice_feat_t feat(float level, float tilt)
{
    voice_feat_t f;
    memset(&f, 0, sizeof(f));
    f.level = level;
    f.tilt_db = tilt;
    return f;
}

static void test_gate()
{
    voice_gate_t g;
    voice_gate_init(&g);
    uint32_t t = 0;
    voice_feat_t f;

    // quiet room: silence, floor settles near the room level
    for (int i = 0; i < 20; i++) { t += 100; f = feat(1.5f, 0); voice_gate_step(&g, &GATE, t, &f); }
    assert(g.state == VOICE_SILENCE);
    assert(g.floor > 1.0f && g.floor < 2.5f);

    // partner at 1 m
    for (int i = 0; i < 5; i++) { t += 100; f = feat(15.0f, -2); assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OTHER); }
    // wearer speaks: OWN at once
    t += 100; f = feat(45.0f, 6);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OWN);
    // soft syllable inside the wearer's utterance, within the hangover: still OWN
    t += 100; f = feat(12.0f, 6);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OWN);
    t += 100; f = feat(12.0f, 6);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OWN);
    // ... but past the hangover, sustained soft speech is somebody else
    t += 100; f = feat(12.0f, 6);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OTHER);
    // silence: state persists through the hangover, then clears
    t += 100; f = feat(1.5f, 0);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OTHER);
    t += 100; f = feat(1.5f, 0);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OTHER);
    t += 100; f = feat(1.5f, 0);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_SILENCE);
    printf("gate: silence / other / own / hangover OK\n");

    // a steady fan is not speech: the floor climbs under it within a minute
    for (int i = 0; i < 600; i++) { t += 100; f = feat(20.0f, 0); voice_gate_step(&g, &GATE, t, &f); }
    printf("gate: floor after 60 s at level 20 = %.1f\n", g.floor);
    assert(g.state == VOICE_SILENCE);
    // ... and one quiet block brings it back down fast enough to hear speech again
    for (int i = 0; i < 4; i++) { t += 100; f = feat(1.5f, 0); voice_gate_step(&g, &GATE, t, &f); }
    assert(g.floor < 3.0f);
    t += 100; f = feat(15.0f, -2);
    assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OTHER);
    printf("gate: floor tracking OK\n");

    // tilt veto, once calibrated: a loud bright voice is a close partner, not the wearer
    voice_gate_cfg_t c = GATE;
    c.own_tilt_min_db = 3.0f;
    voice_gate_init(&g);
    t = 0;
    for (int i = 0; i < 5; i++) { t += 100; f = feat(45.0f, -5); assert(voice_gate_step(&g, &c, t, &f) == VOICE_OTHER); }
    t += 5000; // clear the hangover
    f = feat(1.5f, 0); voice_gate_step(&g, &c, t, &f);
    int own = 0;
    for (int i = 0; i < 5; i++) { t += 100; f = feat(45.0f, 8); if (voice_gate_step(&g, &c, t, &f) == VOICE_OWN) own++; }
    assert(own >= 3); // the smoothed tilt needs a couple of blocks to cross
    printf("gate: tilt veto OK\n");

    // uint32 wrap does not break the hangover
    voice_gate_init(&g);
    t = 0xFFFFFF60u;
    f = feat(45.0f, 6); voice_gate_step(&g, &GATE, t, &f);
    t += 100; f = feat(12.0f, 6); assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OWN);   // 0xFFFFFFC4
    t += 100; f = feat(12.0f, 6); assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OWN);   // 0x00000028
    t += 100; f = feat(12.0f, 6); assert(voice_gate_step(&g, &GATE, t, &f) == VOICE_OTHER);
    printf("gate: wrap OK\n");
}

// ---------------------------------------------------------------------------
// Novelty, driven with hand-made profiles
// ---------------------------------------------------------------------------
static voice_feat_t prof_feat(const float p[VOICE_BANDS], float offset)
{
    voice_feat_t f;
    memset(&f, 0, sizeof(f));
    f.level = 15.0f;
    for (int k = 0; k < VOICE_BANDS; k++) f.band_db[k] = p[k] + offset;
    return f;
}

// feed n OTHER blocks of profile p; return how many candidates fired
static int feed_other(novelty_t *nv, uint32_t *t, const float p[VOICE_BANDS], int n)
{
    int c = 0;
    for (int i = 0; i < n; i++) {
        *t += 100;
        voice_feat_t f = prof_feat(p, 30.0f + (float) (i % 3)); // level offset must not matter
        if (novelty_step(nv, &NOV, *t, VOICE_OTHER, true, &f)) c++;
    }
    return c;
}

static void test_novelty_vectors()
{
    const float P1[VOICE_BANDS] = {3, 2, 0, -2, -3};
    const float P2[VOICE_BANDS] = {-4, -2, 0, 2, 4};
    printf("novelty: P1-P2 distance %.2f dB (threshold %.1f)\n", voice_profile_dist(P1, P2), NOVELTY_DIST_DB);
    assert(voice_profile_dist(P1, P2) > NOVELTY_DIST_DB);

    novelty_t nv;
    novelty_init(&nv);
    uint32_t t = 1000;
    // first voice ever: candidate exactly once, on the block that completes min_blocks
    assert(feed_other(&nv, &t, P1, NOVELTY_MIN_BLOCKS - 1) == 0);
    assert(feed_other(&nv, &t, P1, 1) == 1);
    assert(nv.last_dist < 0.0f); // gallery was empty
    assert(feed_other(&nv, &t, P1, 20) == 0);
    // same voice after a pause: a new segment, matched, no candidate
    t += 3000;
    assert(feed_other(&nv, &t, P1, 15) == 0);
    assert(nv.last_dist < 1.0f);
    // the wearer's blocks are ignored and do not disturb the segment
    voice_feat_t own = prof_feat(P2, 40.0f);
    for (int i = 0; i < 5; i++) { t += 100; assert(!novelty_step(&nv, &NOV, t, VOICE_OWN, true, &own)); }
    assert(feed_other(&nv, &t, P1, 5) == 0);
    // a different voice: candidate
    t += 3000;
    assert(feed_other(&nv, &t, P2, 15) == 1);
    printf("novelty: P2 flagged at distance %.2f\n", nv.last_dist);
    assert(nv.last_dist > NOVELTY_DIST_DB);
    // both are now known
    t += 3000; assert(feed_other(&nv, &t, P1, 15) == 0);
    t += 3000; assert(feed_other(&nv, &t, P2, 15) == 0);
    assert(novelty_live_count(&nv, &NOV, t) == 2);
    // short bursts of speech never reach a score
    t += 3000; assert(feed_other(&nv, &t, P2, 3) == 0);
    t += 3000; assert(feed_other(&nv, &t, P2, 3) == 0);
    // forgotten after a long silence: new again
    t += NOVELTY_FORGET_MS + 1000;
    assert(novelty_live_count(&nv, &NOV, t) == 0);
    assert(feed_other(&nv, &t, P1, 15) == 1);
    printf("novelty: first / known / other / forget OK\n");

    // gallery holds four; a fifth evicts the least recently heard
    const float Q[5][VOICE_BANDS] = {
        {8, 0, 0, 0, -8}, {-8, 0, 0, 0, 8}, {0, 8, 0, -8, 0}, {0, -8, 0, 8, 0}, {0, 0, 0, 0, 0},
    };
    novelty_init(&nv);
    t = 5000;
    const int L = NOVELTY_MIN_BLOCKS + 2;
    for (int i = 0; i < 5; i++) { t += 3000; assert(feed_other(&nv, &t, Q[i], L) == 1); }
    assert(novelty_live_count(&nv, &NOV, t) == 4);
    t += 3000; assert(feed_other(&nv, &t, Q[0], L) == 1); // Q0 was evicted
    t += 3000; assert(feed_other(&nv, &t, Q[2], L) == 0); // Q2 still there
    t += 3000; assert(feed_other(&nv, &t, Q[1], L) == 1); // Q1 went when Q0 came back
    printf("novelty: gallery eviction OK\n");
}

// ---------------------------------------------------------------------------
// End to end: synthetic audio through the filterbank, gate and novelty with
// the shipped defaults. This is the test that decides NOVELTY_DIST_DB.
// ---------------------------------------------------------------------------
struct Rig {
    voice_feat_state_t fs;
    voice_gate_t g;
    novelty_t nv;
    uint32_t now;
    int state;
    int candidates;
    float min_known_dist, max_known_dist; // distances on matched segments
    float min_new_dist;                    // on flagged segments (gallery non-empty)
};

static void rig_init(Rig *r)
{
    voice_feat_init(&r->fs, FS);
    voice_gate_init(&r->g);
    novelty_init(&r->nv);
    r->now = 0;
    r->state = VOICE_SILENCE;
    r->candidates = 0;
    r->min_known_dist = 1e9f;
    r->max_known_dist = -1.0f;
    r->min_new_dist = 1e9f;
}

static void rig_feed(Rig *r, const int16_t *blk)
{
    voice_feat_t f;
    r->now += 100;
    voice_feat_block(&r->fs, blk, N, &f);
    r->state = voice_gate_step(&r->g, &GATE, r->now, &f);
    bool scored_before = r->nv.seg_scored;
    bool cand = novelty_step(&r->nv, &NOV, r->now, r->state, r->g.speech, &f);
    if (cand) {
        r->candidates++;
        if (r->nv.last_dist >= 0.0f && r->nv.last_dist < r->min_new_dist) r->min_new_dist = r->nv.last_dist;
    } else if (!scored_before && r->nv.seg_scored) {
        if (r->nv.last_dist < r->min_known_dist) r->min_known_dist = r->nv.last_dist;
        if (r->nv.last_dist > r->max_known_dist) r->max_known_dist = r->nv.last_dist;
    }
}

static void test_end_to_end()
{
    Rig r;
    rig_init(&r);
    Voice wearer, v1, v2;
    voice_init(&wearer, 110.0f, 14.0f, 11);
    voice_init(&v1, 130.0f, 12.0f, 22);
    voice_init(&v2, 210.0f, 6.0f, 33);
    uint32_t nseed = 44;
    int16_t blk[N];
    int own_blocks = 0, other_blocks = 0;

    // 2 s room tone
    for (int i = 0; i < 20; i++) { noise_block(1.5f, blk, &nseed); rig_feed(&r, blk); }
    assert(r.state == VOICE_SILENCE);

    // partner 1 starts talking: the first voice of the day is a candidate
    for (int i = 0; i < 15; i++) { voice_block(&v1, 15.0f, blk); rig_feed(&r, blk); if (r.state == VOICE_OTHER) other_blocks++; }
    assert(other_blocks >= 14);
    assert(r.candidates == 1);

    // wearer replies: OWN, never a candidate
    for (int i = 0; i < 15; i++) { voice_block(&wearer, 45.0f, blk); rig_feed(&r, blk); if (r.state == VOICE_OWN) own_blocks++; }
    assert(own_blocks >= 14);
    assert(r.candidates == 1);

    // pause, then partner 1 again: known
    for (int i = 0; i < 20; i++) { noise_block(1.5f, blk, &nseed); rig_feed(&r, blk); }
    for (int i = 0; i < 15; i++) { voice_block(&v1, 15.0f, blk); rig_feed(&r, blk); }
    assert(r.candidates == 1);

    // partner 2 joins: new
    for (int i = 0; i < 20; i++) { noise_block(1.5f, blk, &nseed); rig_feed(&r, blk); }
    for (int i = 0; i < 15; i++) { voice_block(&v2, 15.0f, blk); rig_feed(&r, blk); }
    assert(r.candidates == 2);

    // they alternate a few times: nothing new
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 20; i++) { noise_block(1.5f, blk, &nseed); rig_feed(&r, blk); }
        for (int i = 0; i < 16; i++) { voice_block(&v1, 15.0f, blk); rig_feed(&r, blk); }
        for (int i = 0; i < 10; i++) { voice_block(&wearer, 45.0f, blk); rig_feed(&r, blk); }
        for (int i = 0; i < 20; i++) { noise_block(1.5f, blk, &nseed); rig_feed(&r, blk); }
        for (int i = 0; i < 16; i++) { voice_block(&v2, 12.0f, blk); rig_feed(&r, blk); } // a little quieter
    }
    assert(r.candidates == 2);

    printf("end-to-end: same-voice distances %.2f..%.2f dB, new-voice min %.2f dB, threshold %.1f\n",
           r.min_known_dist, r.max_known_dist, r.min_new_dist, NOVELTY_DIST_DB);
    // margin both ways: the threshold should sit inside the gap. Synthetic
    // same-voice spread is optimistic; the new-voice side is the one that
    // must hold, since a missed new voice is a lost photo.
    assert(r.max_known_dist < NOVELTY_DIST_DB * 0.5f);
    assert(r.min_new_dist > NOVELTY_DIST_DB * 1.3f);
    printf("end-to-end OK\n");
}

// ---------------------------------------------------------------------------
// Thermal
// ---------------------------------------------------------------------------
static void test_thermal()
{
    const thermal_cfg_t c = {THERMAL_WARM_C, THERMAL_HOT_C, THERMAL_HYST_C};
    thermal_t t;
    thermal_init(&t);
    assert(thermal_step(&t, &c, 55.0f) == THERMAL_NORMAL);
    assert(thermal_step(&t, &c, 60.0f) == THERMAL_WARM);
    assert(thermal_step(&t, &c, 57.0f) == THERMAL_WARM); // inside hysteresis
    assert(thermal_step(&t, &c, 54.0f) == THERMAL_NORMAL);
    assert(thermal_step(&t, &c, 75.0f) == THERMAL_WARM); // one step per sample
    assert(thermal_step(&t, &c, 75.0f) == THERMAL_HOT);
    assert(thermal_step(&t, &c, 66.0f) == THERMAL_HOT);
    assert(thermal_step(&t, &c, 64.0f) == THERMAL_WARM);
    assert(thermal_step(&t, &c, 54.0f) == THERMAL_NORMAL);
    printf("thermal hysteresis OK\n");
}

int main()
{
    test_hp();
    test_bands();
    test_gate();
    test_novelty_vectors();
    test_end_to_end();
    test_thermal();
    printf("ALL VOICE TESTS PASSED\n");
    return 0;
}
