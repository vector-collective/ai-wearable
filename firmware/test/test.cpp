// Logic test for mic.cpp source selection, run on the host against stub drivers.
#include "Arduino.h"
#include "driver/i2s.h"
#include "mic.h"
#include "config.h"
#include "voice_logic.h"

#include <cassert>
#include <cmath>
#include <vector>

uint32_t g_fake_millis = 0;
SerialStub Serial;
int32_t g_chan_amp[2][2] = {{0, 0}, {0, 0}};
bool g_bus_fail[2] = {false, false};

static std::vector<int16_t> last_block;
static void capture(int16_t *data, size_t samples)
{
    last_block.assign(data, data + samples);
}

static int voice_events = 0;
static void on_voice(uint8_t event, int16_t arg)
{
    printf("voice event %u arg %d\n", event, arg);
    voice_events++;
}

// Set a channel's synthetic level. "amp16" is the desired post-shift int16
// magnitude; the raw slot value is amp16 << MIC_BIT_SHIFT. The stub emits it
// as a square wave at Nyquist, which the high-pass passes at unity, so the
// emitted block's mean-abs level equals amp16.
static void set_amp(int port, int slot, int amp16)
{
    g_chan_amp[port][slot] = ((int32_t) amp16) << MIC_BIT_SHIFT;
}

// Mean-abs level of the last emitted block.
static float block_level()
{
    assert(!last_block.empty());
    double acc = 0.0;
    for (int16_t v : last_block) acc += std::abs((int) v);
    return (float) (acc / last_block.size());
}

static bool near(float v, float target, float tol_frac = 0.02f)
{
    float tol = fabsf(target) * tol_frac;
    if (tol < 0.5f) tol = 0.5f;
    return fabsf(v - target) <= tol;
}

static void run_blocks(int n)
{
    for (int i = 0; i < n; i++) {
        g_fake_millis += 100;
        mic_process();
    }
}

int main()
{
    assert(mic_start());
    mic_set_callback(capture);
    mic_set_voice_callback(on_voice);

    // Levels are post->>16 mean-abs. Rule of thumb: a partner at 1 m is ~15,
    // room noise ~1.5, the wearer ~40. MIC_SWITCH_SILENCE_LEVEL is 8.

    // 1) Only case mic A active -> output follows A
    set_amp(0, 0, 250); // A = bus0 slot0
    set_amp(0, 1, 2);   // B quiet
    set_amp(1, 0, 2);   // C quiet
    set_amp(1, 1, 0);   // lapel absent (pulled-down line reads 0)
    run_blocks(2);
    printf("after A-loud: level=%.1f\n", block_level());
    assert(near(block_level(), 250));

    // 2) B becomes much louder, but everyone is above the silence gate:
    // changeover must be suppressed rather than splicing mid-utterance.
    set_amp(0, 0, 25);
    set_amp(0, 1, 500);
    run_blocks(6);
    printf("mid-speech, B much louder: level=%.1f (expect 25, no switch)\n", block_level());
    assert(near(block_level(), 25)); // still A

    // 3) A quiet gap with B still ahead: now the changeover is allowed, and
    // it still needs MIC_SWITCH_BLOCKS consecutive blocks of hysteresis.
    set_amp(0, 0, 1);
    set_amp(0, 1, 6);
    run_blocks(2); // < MIC_SWITCH_BLOCKS
    assert(near(block_level(), 1)); // still A
    run_blocks(2); // crosses the threshold during silence
    printf("after quiet gap: level=%.1f\n", block_level());
    assert(near(block_level(), 6)); // switched to B

    // ...and it stays on B once speech resumes
    set_amp(0, 0, 25);
    set_amp(0, 1, 500);
    run_blocks(2);
    assert(near(block_level(), 500));

#if MIC_LAPEL_FITTED
    // 4) Lapel plugged in and speaking -> takes over immediately
    set_amp(1, 1, 225);
    run_blocks(1);
    printf("after lapel-live: level=%.1f\n", block_level());
    assert(near(block_level(), 225));

    // 5) Lapel goes silent -> hold keeps it selected ~5s, then falls back to B
    set_amp(1, 1, 0);
    run_blocks(40); // 4.0s of silence: still inside hold
    assert(block_level() < 0.5f);
    run_blocks(15); // total 5.5s: hold expired
    printf("after lapel-silent 5.5s: level=%.1f\n", block_level());
    assert(near(block_level(), 500)); // back on case mic B

#else
    // Phase 1 ships with MIC_LAPEL_FITTED 0: the lapel slot is never sampled
    // or scored, so an unconnected (or noisy) input cannot hijack the source
    // no matter how loud it reads.
    set_amp(1, 1, 9000);
    run_blocks(4);
    printf("lapel not fitted, lapel slot loud: level=%.1f (expect 500)\n", block_level());
    assert(near(block_level(), 500));
    set_amp(1, 1, 0);
#endif

    // 6) Mic C wins when it leads during a quiet interval
    set_amp(0, 0, 1);
    set_amp(0, 1, 1);
    set_amp(1, 0, 6);
    run_blocks(4);
    assert(near(block_level(), 6)); // switched to C while quiet
    set_amp(1, 0, 750);             // speech resumes on C
    run_blocks(2);
    printf("after C-loud: level=%.1f\n", block_level());
    assert(near(block_level(), 750));

    // 7) Full-scale input neither wraps nor overshoots
    g_chan_amp[1][0] = INT32_MAX;
    run_blocks(4);
    printf("after saturation: level=%.1f\n", block_level());
    assert(near(block_level(), 32767, 0.01f));

    // 8) The voice gate sees the emitted stream: full scale is OWN, a quiet
    // channel is silence. (A Nyquist square has no energy in the bands, so
    // the novelty detector never scores here - that path has its own tests.)
    assert(mic_voice_state() == VOICE_OWN);
    set_amp(1, 0, 1);
    run_blocks(10);
    assert(mic_voice_state() == VOICE_SILENCE);
    assert(voice_events == 0);

    // 9) Bus1 install failure degrades gracefully to case pair
    mic_stop();
    g_bus_fail[1] = true;
    assert(mic_start());
    mic_set_callback(capture);
    set_amp(0, 0, 175);
    set_amp(0, 1, 7);
    run_blocks(4);
    printf("bus1-failed fallback: level=%.1f\n", block_level());
    assert(near(block_level(), 175));
    mic_stop();

    printf("ALL TESTS PASSED\n");
    return 0;
}
