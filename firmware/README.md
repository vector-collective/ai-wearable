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

## Wiring (final — all 11 header pins allocated)

| Pin | GPIO | Function |
|---|---|---|
| D0  | 1  | Case button (momentary, to GND) |
| D1  | 2  | **Dual use**: WS2812 LED data-in + battery divider node |
| D2  | 3  | Bus 1 SCK (rear mic C + lapel D) |
| D3  | 4  | Bus 1 WS |
| D4  | 5  | Bus 1 SD |
| D5  | 6  | Bus 0 SCK (case mics A + B) |
| D6  | 43 | Bus 0 WS |
| D7  | 44 | Bus 0 SD |
| D8  | 7  | microSD SPI SCK (hardwired on Sense board) |
| D9  | 8  | microSD SPI MISO (hardwired) |
| D10 | 9  | microSD SPI MOSI (hardwired; CS is GPIO21 internally) |

Per-mic L/R select: **A** (front-up) and **C** (rear) tie L/R → GND;
**B** (out-up) and lapel **D** tie L/R → 3V3. The lapel connector carries
bus 1's SCK/WS/SD plus 3V3/GND (5 pins).

**Battery divider** (enables the LED battery gauge): BAT+ —[100kΩ]— D1 node
—[100kΩ]— GND. The WS2812's DIN connects to the same D1 node; power the
WS2812 from the battery rail (3.5–4.2V is in spec), GND common.

**Power off** is the expansion base's physical battery switch — the old
2s-hold firmware power-off is gone; long press now belongs to recording.

## Case controls (button + RGB LED)

| Context | Input | Action | LED |
|---|---|---|---|
| Idle | short press | battery check | solid 2s, cool→warm = full→low, red = almost dead, orange = swap now |
| Idle | long press (1.5s, deliberate) | start recording | 1 blink in battery color |
| Recording | short press | bookmark + segment split | 1 cyan blink |
| Recording | long press | stop recording | 2 blinks in SD-free-space color (same spectrum) |
| — | SD mount fails on start | stays idle | 3 fast red blinks |

Recording writes to the Sense microSD per session:
`/rec/S0001/seg01/f000000.jpg…` (JPEG frames at `VIDEO_FPS`, default 5) +
`audio.wav` (selected-mic mono 16k) per segment, and `bookmarks.csv`
(millis, segment, frame). Each bookmark closes the current segment and opens
the next, so bookmarks are also clean file boundaries. WAV headers are
re-patched every 5s, so a crash still leaves playable audio.

Assemble a segment into a normal video on the server:

```bash
ffmpeg -framerate 5 -i seg01/f%06d.jpg -i seg01/audio.wav \
  -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest seg01.mp4
```

**GPIO21 caveat:** the SD's chip select shares the net with the onboard
orange LED. Once the SD has been mounted, the firmware stops driving the
status LED entirely (`statusLedWrite` guard) — the WS2812 is the sole
indicator from then on.

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
