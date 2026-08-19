# Belt-Worn Audio Capture — Quad INMP441 Firmware

Patched [omiGlass firmware](https://github.com/BasedHardware/omi/tree/main/omiGlass/firmware)
(BasedHardware/omi @ `1cc793ce2ec9`, MIT — see `LICENSE.upstream`) for the
belt-worn capture device: XIAO ESP32S3 + four INMP441 I2S mics, streaming
Opus over BLE to the Omi phone app.

**What changed vs upstream** (full diff in `patches/quad-inmp441.patch`,
touching only `src/mic.cpp` and `src/config.h`):

- PDM capture (XIAO Sense onboard mic) replaced with **two standard-mode
  I2S buses, stereo, 32-bit slots** for four INMP441s.
- **Source selection**: per 100ms block, mean-abs level per channel; the
  lapel wins while it carries signal (with a 5s hold so speech pauses don't
  bounce it), otherwise the loudest case mic wins with 3-blocks-at-1.5x
  hysteresis. Selected channel is emitted mono into the unchanged
  Opus/BLE pipeline — `app.cpp` and the Omi protocol are untouched.
- **Lapel is optional and hot-pluggable**: its data line is pulled down, so
  an unplugged lapel reads as silence and selection falls back to the case
  mics automatically. Plugging it back in re-activates it within a block.
- **Selection logging** over USB serial: every source change, plus a levels
  line every 10s (`MIC: levels A=.. B=.. C=.. D=..`) — this is the data for
  deciding how often the lapel actually earns its keep.

## Wiring (final, supersedes earlier drafts)

GPIO1 (D0) and GPIO2 (D1) are **reserved by this firmware** for the power
button and battery ADC — bus 1 therefore lives on D2–D4.

| Signal | Case pair (bus 0) | Rear + lapel (bus 1) |
|---|---|---|
| SCK    | D8 (GPIO7)  | D2 (GPIO3) |
| WS     | D9 (GPIO8)  | D3 (GPIO4) |
| SD     | D10 (GPIO9) | D4 (GPIO5) |
| VDD    | 3V3         | 3V3        |
| GND    | GND         | GND        |

Per-mic L/R select: **A** (front-up) and **C** (rear) tie L/R → GND;
**B** (out-up) and lapel **D** tie L/R → 3V3. The lapel connector carries
bus 1's SCK/WS/SD plus 3V3/GND (5 pins).

Optional extras the stock firmware already supports: momentary power button
on D0→GND (2s long-press = off), status LED is the onboard GPIO21 LED.

## Build & flash

```bash
pip install platformio
cd firmware
pio run -e seeed_xiao_esp32s3 -t upload   # board on USB-C
pio device monitor                        # watch mic logs at 115200
```

No camera board required — camera init failure is non-fatal upstream and
audio runs regardless.

## Bring-up checklist

1. Flash, open serial monitor. Expect `Initializing quad INMP441 I2S
   capture...` and both bus pin reports.
2. Tap each mic in turn; watch the `MIC:` lines follow your taps (A, B, C,
   then plug the lapel and tap D). If a pair is reversed, set
   `MIC_BUS0_SWAP_LR` / `MIC_BUS1_SWAP_LR` to 1 in `config.h` and reflash.
3. If audio is quiet in the Omi app, lower `MIC_BIT_SHIFT` to 13 (doubles
   gain) before touching `MIC_GAIN`.
4. Pair in the Omi app as usual — the device advertises the standard OMI
   service; nothing app-side knows or cares that the mics changed.

## Tuning knobs (`src/config.h`)

| Define | Default | Meaning |
|---|---|---|
| `MIC_BIT_SHIFT` | 14 | 32-bit slot → int16; smaller = louder (13 ≈ 2x) |
| `MIC_LAPEL_PRESENT_LEVEL` | 40 | Level that marks the lapel live |
| `MIC_LAPEL_HOLD_MS` | 5000 | Lapel stays selected this long after signal |
| `MIC_SWITCH_RATIO_*`, `MIC_SWITCH_BLOCKS` | 3/2, 3 | Case-mic switch hysteresis |
| `MIC_STATS_INTERVAL_MS` | 10000 | Cadence of the levels log line |
