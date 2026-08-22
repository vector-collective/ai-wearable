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

## Wiring (all 11 header pins allocated)

> **Do not use the Seeed "Expansion Board Base for XIAO."** It is not a passive
> battery holder: per [Seeed's docs](https://wiki.seeedstudio.com/Seeeduino-XIAO-Expansion-Board/)
> it hardwires a buzzer to A3, a user button to D1, an OLED/RTC I2C bus to
> D4/D5 and its own microSD chip-select to D2 — colliding with the LED node
> and all of mic bus 1. Solder the battery to the XIAO's own **B+/B− pads**
> instead; the XIAO ESP32S3 has onboard lithium charge management.

| Pin | GPIO | Function |
|---|---|---|
| D0  | 1  | **Analog node**: battery divider + case button (see below) |
| D1  | 2  | WS2812 LED data-in (only) |
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

**Battery + button share one analog node (D0/GPIO1).** Both functions are
high-impedance analog, so unlike the earlier LED/ADC arrangement they don't
conflict:

```
BAT+ --[100k]--+-- GPIO1
               |
              [100k]      <- momentary switch wired ACROSS this resistor
               |
              GND         plus 100nF from GPIO1 to GND
```

Released, the node sits at VBAT/2 (battery reading). Pressed, it's pulled to
~0V. One ADC read serves both. The 100nF settles the ADC sample-and-hold
*and* debounces the switch for free.

**Power the WS2812 from the 3V3 rail, not the battery rail.** WS2812B needs
V_IH ≥ 0.7·VDD; on a 4.2V pack that's 2.94V, which a 3.3V GPIO cannot
guarantee (ESP32-S3 worst-case V_OH is 2.64V). At 3.3V VDD the threshold
drops to 2.31V with comfortable margin, and it stops moving as the pack
drains. Slightly dimmer, reliably correct.

**Power off** is the inline battery switch — the old 2s-hold firmware
power-off is gone; long press belongs to recording.

### Passive components (all cheap, all worth fitting)

| Where | Part | Why |
|---|---|---|
| GPIO1 → GND | 100nF | ADC settling + button debounce |
| GPIO2 → WS2812 DIN | 330–470Ω series at the MCU | Edge damping, ESD limiting |
| WS2812 DIN → GND | 10kΩ | Defined level before `ui_init()` runs |
| WS2812 VDD/GND | 100nF + 10µF | Local decoupling |
| Bus SCK/WS at MCU | 68–100Ω series each | Series termination; also call `gpio_set_drive_capability(..., GPIO_DRIVE_CAP_0)` |
| Each mic's SD output | 100Ω series | Limits hot-plug contention on the shared data line |
| GPIO5 → GND | 10kΩ | External pull-down; the internal ~45kΩ is weak for 50cm of cable |
| Lapel pod | 100nF + 10µF, 10Ω in series with its 3V3 | Decoupling + inrush damping at the far end |
| BAT+ | P-channel MOSFET (DMG2301L class) | Reverse-polarity protection — JST wire colors are not standardized |
| Battery rail | 10µF | Bulk |
| Lapel connector | 4-ch TVS array (SRV05-4 class) | Exposed pins on a worn device get touched |

Lapel cable: 28 AWG stranded, shielded or twisted pairs, shield grounded at
the XIAO end only. Order the connector **3V3 / SCK / GND / SD / WS** so the
ground sits between clock and data.

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
