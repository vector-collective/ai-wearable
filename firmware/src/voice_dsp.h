// Pure DSP for the mic path - no hardware includes, host-testable.
//
// Two things live here:
//   1. a first-order high-pass that runs on every channel before the level
//      metric and before the stream leaves the device (clothing rumble, body
//      movement and wind sit below 100 Hz and used to dominate the mean-abs
//      level the source selector keys on, and cost Opus bits for nothing);
//   2. a five-band octave filterbank that turns each 100 ms block of the
//      selected mono stream into a coarse spectral profile. voice_logic.h
//      uses the level and tilt for the own-voice gate and the band profile
//      for the device-tier new-voice detector.
//
// Everything is single-precision float: the ESP32-S3 has an FPU, and the
// whole bank costs well under 1% of an 80 MHz core at 16 kHz.
#ifndef VOICE_DSP_H
#define VOICE_DSP_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef VOICE_PI
#define VOICE_PI 3.14159265358979f
#endif

// ---------------------------------------------------------------------------
// First-order high-pass, bilinear transform with frequency pre-warp, so the
// -3 dB point lands exactly on fc:  H(z) = K (1 - z^-1) / (1 - a z^-1)
// ---------------------------------------------------------------------------
typedef struct {
    float a;  // pole
    float k;  // gain, (1 + a) / 2, unity at Nyquist
    float x1; // previous input
    float y1; // previous output
} hp1_t;

static inline void hp1_init(hp1_t *f, float fc_hz, float fs_hz)
{
    if (fc_hz <= 0.0f) {
        // Disabled: pass-through.
        f->a = 0.0f;
        f->k = 1.0f;
        f->x1 = 0.0f;
        f->y1 = 0.0f;
        return;
    }
    float t = tanf(VOICE_PI * fc_hz / fs_hz);
    f->a = (1.0f - t) / (1.0f + t);
    f->k = 1.0f / (1.0f + t);
    f->x1 = 0.0f;
    f->y1 = 0.0f;
}

static inline float hp1_step(hp1_t *f, float x)
{
    if (f->a == 0.0f) {
        return x;
    }
    float y = f->k * (x - f->x1) + f->a * f->y1;
    f->x1 = x;
    f->y1 = y;
    return y;
}

// ---------------------------------------------------------------------------
// RBJ band-pass biquad (constant 0 dB peak gain), direct form II transposed.
// ---------------------------------------------------------------------------
typedef struct {
    float b0, b2, a1, a2; // b1 is zero for this topology
    float s1, s2;
} bpf_t;

static inline void bpf_init(bpf_t *f, float f0_hz, float q, float fs_hz)
{
    float w0 = 2.0f * VOICE_PI * f0_hz / fs_hz;
    float alpha = sinf(w0) / (2.0f * q);
    float a0 = 1.0f + alpha;
    f->b0 = alpha / a0;
    f->b2 = -alpha / a0;
    f->a1 = -2.0f * cosf(w0) / a0;
    f->a2 = (1.0f - alpha) / a0;
    f->s1 = 0.0f;
    f->s2 = 0.0f;
}

static inline float bpf_step(bpf_t *f, float x)
{
    float y = f->b0 * x + f->s1;
    f->s1 = -f->a1 * y + f->s2; // b1 == 0
    f->s2 = f->b2 * x - f->a2 * y;
    return y;
}

// ---------------------------------------------------------------------------
// Block features
// ---------------------------------------------------------------------------
#define VOICE_BANDS 5

// Octave bands. The top band reaches Nyquist at 16 kHz; the bottom one sits
// above the high-pass so rumble never leaks into the profile.
static const float VOICE_BAND_HZ[VOICE_BANDS] = {250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f};
#ifndef VOICE_BAND_Q
#define VOICE_BAND_Q 1.414f // one octave wide
#endif

typedef struct {
    bpf_t band[VOICE_BANDS];
} voice_feat_state_t;

typedef struct {
    float level;                // mean |x| over the block, int16 units
    float band_db[VOICE_BANDS]; // 10 log10 of mean band energy
    float tilt_db;              // low bands minus high bands: positive = dark
} voice_feat_t;

static inline void voice_feat_init(voice_feat_state_t *s, float fs_hz)
{
    for (int k = 0; k < VOICE_BANDS; k++) {
        bpf_init(&s->band[k], VOICE_BAND_HZ[k], VOICE_BAND_Q, fs_hz);
    }
}

static inline void voice_feat_block(voice_feat_state_t *s, const int16_t *x, size_t n, voice_feat_t *out)
{
    float acc_abs = 0.0f;
    float acc_e[VOICE_BANDS];
    for (int k = 0; k < VOICE_BANDS; k++) {
        acc_e[k] = 0.0f;
    }
    for (size_t i = 0; i < n; i++) {
        float v = (float) x[i];
        acc_abs += (v < 0.0f) ? -v : v;
        for (int k = 0; k < VOICE_BANDS; k++) {
            float y = bpf_step(&s->band[k], v);
            acc_e[k] += y * y;
        }
    }
    float inv_n = (n > 0) ? 1.0f / (float) n : 0.0f;
    out->level = acc_abs * inv_n;
    for (int k = 0; k < VOICE_BANDS; k++) {
        // +1e-3 keeps digital silence finite (-30 dB) instead of -inf
        out->band_db[k] = 10.0f * log10f(acc_e[k] * inv_n + 1e-3f);
    }
    out->tilt_db = 0.5f * (out->band_db[0] + out->band_db[1]) - 0.5f * (out->band_db[3] + out->band_db[4]);
}

// Level-independent profile: band dB minus their mean. What the novelty
// detector compares.
static inline void voice_profile(const voice_feat_t *f, float out[VOICE_BANDS])
{
    float mean = 0.0f;
    for (int k = 0; k < VOICE_BANDS; k++) {
        mean += f->band_db[k];
    }
    mean /= (float) VOICE_BANDS;
    for (int k = 0; k < VOICE_BANDS; k++) {
        out[k] = f->band_db[k] - mean;
    }
}

// RMS distance in dB between two profiles.
static inline float voice_profile_dist(const float a[VOICE_BANDS], const float b[VOICE_BANDS])
{
    float acc = 0.0f;
    for (int k = 0; k < VOICE_BANDS; k++) {
        float d = a[k] - b[k];
        acc += d * d;
    }
    return sqrtf(acc / (float) VOICE_BANDS);
}

#endif // VOICE_DSP_H
