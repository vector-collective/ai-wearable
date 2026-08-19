// Logic test for mic.cpp source selection, run on the host against stub drivers.
#include "Arduino.h"
#include "driver/i2s.h"
#include "mic.h"
#include "config.h"

#include <cassert>
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

// Set a channel's synthetic level. "amp16" is the desired post-shift int16
// magnitude; the raw slot value is amp16 << MIC_BIT_SHIFT.
static void set_amp(int port, int slot, int amp16)
{
    g_chan_amp[port][slot] = ((int32_t) amp16) << MIC_BIT_SHIFT;
}

static int16_t block_value()
{
    assert(!last_block.empty());
    return last_block[0];
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

    // 1) Only case mic A active -> output follows A
    set_amp(0, 0, 1000); // A = bus0 slot0
    set_amp(0, 1, 10);   // B quiet
    set_amp(1, 0, 10);   // C quiet
    set_amp(1, 1, 0);    // lapel absent (pulled-down line reads 0)
    run_blocks(2);
    printf("after A-loud: sample=%d\n", block_value());
    assert(block_value() == 1000);

    // 2) B becomes much louder; must NOT switch before hysteresis satisfied
    set_amp(0, 0, 100);
    set_amp(0, 1, 2000);
    run_blocks(2); // 2 blocks < MIC_SWITCH_BLOCKS
    assert(block_value() == 100); // still A
    run_blocks(2); // crosses 3-block threshold
    printf("after B-loud x4 blocks: sample=%d\n", block_value());
    assert(block_value() == 2000); // switched to B

    // 3) Lapel plugged in and speaking -> takes over immediately
    set_amp(1, 1, 900);
    run_blocks(1);
    printf("after lapel-live: sample=%d\n", block_value());
    assert(block_value() == 900);

    // 4) Lapel goes silent -> hold keeps it selected ~5s, then falls back to B
    set_amp(1, 1, 0);
    run_blocks(40); // 4.0s of silence: still inside hold
    assert(block_value() == 0);
    run_blocks(15); // total 5.5s: hold expired
    printf("after lapel-silent 5.5s: sample=%d\n", block_value());
    assert(block_value() == 2000); // back on case mic B

    // 5) Rear mic C wins when loudest
    set_amp(0, 1, 50);
    set_amp(1, 0, 3000);
    run_blocks(4);
    printf("after C-loud: sample=%d\n", block_value());
    assert(block_value() == 3000);

    // 6) Saturation clamps instead of wrapping
    g_chan_amp[1][0] = INT32_MAX;
    run_blocks(4);
    printf("after saturation: sample=%d\n", block_value());
    assert(block_value() == 32767);

    // 7) Bus1 install failure degrades gracefully to case pair
    mic_stop();
    g_bus_fail[1] = true;
    assert(mic_start());
    mic_set_callback(capture);
    set_amp(0, 0, 700);
    set_amp(0, 1, 30);
    run_blocks(4);
    printf("bus1-failed fallback: sample=%d\n", block_value());
    assert(block_value() == 700);
    mic_stop();

    printf("ALL TESTS PASSED\n");
    return 0;
}
