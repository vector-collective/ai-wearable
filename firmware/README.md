# Pendant Capture Firmware

Patched [omiGlass firmware](https://github.com/BasedHardware/omi/tree/main/omiGlass/firmware)
(BasedHardware/omi @ `1cc793ce2ec9`, MIT — see `LICENSE.upstream`) for a
chest-worn pendant: XIAO ESP32S3 Sense + three INMP441 I2S microphones,
streaming Opus over BLE to the phone for live transcription, with a
button-toggled local video+audio session on the microSD, a new-voice photo
burst, and an on-device own-voice gate. What it is all for is in
[`docs/SPEC.md`](../docs/SPEC.md).

Audio capture is **always on**. Video is the only thing the button toggles,
and audio is written to SD only while a video session runs, so the two can
be joined afterwards.

**What changed vs upstream** (full diff in `patches/quad-inmp441.patch`):

- **Microphones**: PDM capture (the Sense board's onboard mic) replaced with
  two standard-mode I2S buses, stereo, 32-bit slots, for up to four
  INMP441s (phase 1 fits three; see below). Per 100ms block the firmware
  measures each channel and emits one as mono into the unchanged Opus/BLE
  pipeline. Changeover happens only during near-silence — switching
  mid-utterance splices two different room responses together, which clicks
  and degrades speaker attribution downstream. A lapel pod, when fitted,
  wins while it carries signal.
- **Case UI** (`ui.cpp`, `ui_logic.h`): button + WS2812 RGB LED state
  machine for the battery readout and AV capture control.
- **Recorder** (`sd_recorder.cpp`, `recorder_util.h`): a FreeRTOS task that
  owns the camera and SD card. Camera grabs and card writes routinely stall
  for 100–500ms, so keeping them off the audio path is what allows
  full-resolution capture without dropping samples.
- **Camera lifecycle**: powered up on video-session start, powered down on
  stop. Streaming stills to the phone is disabled — this build has no use
  for them and they contended for the frame buffer.
- **Selection logging** over USB serial: every source change, plus a levels
  line every 10s (`MIC: levels A=.. B=.. C=..`) — bring-up instrument and
  the data for deciding how much each mic actually earns its keep.

Everything is host-testable: `./test/run.sh` compiles the real `mic.cpp`
against stub drivers with plain g++, no ESP toolchain needed, and runs the
mic suite against both lapel configurations plus the UI suite.

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
| D2  | 3  | Bus 1 SCK (mic C + lapel D) |
| D3  | 4  | Bus 1 WS |
| D4  | 5  | Bus 1 SD |
| D5  | 6  | Bus 0 SCK (mics A + B) |
| D6  | 43 | Bus 0 WS |
| D7  | 44 | Bus 0 SD |
| D8  | 7  | microSD SPI SCK (hardwired on Sense board) |
| D9  | 8  | microSD SPI MISO (hardwired) |
| D10 | 9  | microSD SPI MOSI (hardwired; CS is GPIO21 internally) |

Per-mic L/R select: **A** and **C** tie L/R → GND; **B** and lapel **D**
tie L/R → 3V3. Which port on the enclosure is A, B or C is an assembly
choice recorded in that enclosure's README (`hardware/medallion`,
`hardware/pouch`); the firmware only needs the names to match the wiring.
The lapel connector carries bus 1's SCK/WS/SD plus 3V3/GND (5 pins).

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

## Two capture paths

**Audio is always on.** From boot, the mics capture continuously and stream
Opus over BLE to the phone for live transcription. Nothing gates this — no
button, no toggle. This is the path the word-choice and recall work depends
on, so it must never depend on remembering to start it.

**Video is the only thing the button toggles**, and audio is written to the
SD card *only* while a video session is running, so the two can be joined
afterwards. Outside a video session, audio exists solely as the live
transcription stream.

**The LED is off at all times except while showing a readout**, plus a dim
heartbeat while a capture is running (below).

| Context | Input | Action | LED |
|---|---|---|---|
| Idle | short press | battery readout | **5 blinks** in battery color: cool→warm = full→low, orange = swap now, red = almost dead |
| Idle | long press (1.5s, deliberate) | **start AV capture** | 1 blink in battery color |
| Recording | short press | **stop AV capture** | **2 blinks** in SD-free-space color (same spectrum) |
| Recording | long press | stop AV capture | same |
| — | SD mount fails on start | stays idle | 3 fast red blinks |

A quick press does one of two things depending on state, and **the blink
count tells you which**: five blinks means the device was idle and this is a
battery reading; two means it was recording and the capture has just stopped.
Both use the same cool→warm spectrum, so the count is what disambiguates
them. Counts are `UI_BATTERY_BLINKS` and `UI_REC_STOP_BLINKS` in `config.h`.

Either press stops a running capture: a held press must not be a dead
gesture. Starting still requires the deliberate 1.5s hold, so the guarded
control is the one that begins recording, not the one that ends it.

**Recording heartbeat.** While a capture is running the LED gives a single
dim blink in the battery colour every 30s (`UI_REC_HEARTBEAT_MS`), so a
session you forgot to stop is discoverable at a glance — and the colour
tells you whether the cell will see it through. This is also how you read
the battery mid-capture, since a quick press would stop the recording. It only fires when the LED
is otherwise idle, so it can never truncate a battery or SD readout. The dim
fraction is `UI_REC_HEARTBEAT_NUM`/`UI_REC_HEARTBEAT_DEN` (default 1/4) of
the already brightness-scaled colour; raise it on the bench if it is too
faint through your enclosure, lower it if it is conspicuous.

A session writes to the Sense microSD: `/rec/S0001/seg01/f000000.jpg…` (one
frame per `VIDEO_FRAME_INTERVAL_MS`, default 30s) plus `audio.wav` (mono
16k). WAV headers are re-patched every 5s, so a crash still leaves playable
audio, and dropped audio bytes are counted and reported at session end.

One frame per 30s is deliberate, not a limitation: it matches the SenseCam
evidence base for photo-cued recall while costing ~1/150th the SD and CPU
load of 5fps — load that would otherwise compete with the audio it is meant
to accompany. The camera is idle outside video sessions (streaming stills to
the phone is disabled), so it costs nothing the rest of the time.

Assemble a segment on the server (`1/30` = one frame per 30 seconds):

```bash
ffmpeg -framerate 1/30 -i seg01/f%06d.jpg -i seg01/audio.wav \
  -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest seg01.mp4
```

## Flash budget

The board has 8MB of flash. Upstream's partition table allocated only the
first 4MB, capping each OTA slot at 1.75MB — which this build filled to
95.5%, leaving 83kB of headroom and no room to grow. `partitions_ota.csv`
now spans the whole chip: 3.625MB per OTA slot plus 704kB of SPIFFS, which
puts the firmware at roughly 46% of its partition.

Watch the `Flash: [====]` line in the CI build output when adding features.

## Phase 1: three case mics, no lapel

Phase 1 builds **without the lapel pod**. The connector, wiring and firmware
path are provisioned; the microphone is not fitted. `MIC_LAPEL_FITTED 0` in
`config.h` means the lapel slot is never sampled, never scored and never
selectable — an unconnected or noisy input cannot influence the source
choice however loud it reads — and the serial logs print only the three real
channels. Set it to `1` when the pod exists; nothing else changes.

That leaves three microphones, all on the pendant's front face: **A**,
**B**, **C**. C still shares bus 1 with the lapel's reserved slot, so no
wiring changes when the pod arrives.

Honest note on what this costs: fitting the lapel later is the single
largest available improvement to transcript quality — roughly 10–20 dB of
SNR against case mics at the chest, more than every firmware change here
combined. Phase 1 is expected to transcribe acceptably at conversational
distance in a quiet room, which is the stated use, and to degrade in noise
or across a large room in a way the lapel would fix.

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

The camera is not touched at boot, so a board without the Sense
daughterboard still streams audio normally — video sessions simply report
that the camera failed to start and record audio only.

## Bring-up checklist

1. Flash, open serial monitor. Expect `Initializing INMP441 I2S
   capture...` and both bus pin reports.
2. Tap each mic in turn; watch the `MIC:` lines follow your taps (A, B, C,
   then plug the lapel and tap D). If a pair is reversed, set
   `MIC_BUS0_SWAP_LR` / `MIC_BUS1_SWAP_LR` to 1 in `config.h` and reflash.
3. The stream is deliberately 12 dB quieter than it was at `MIC_BIT_SHIFT`
   14: the headroom now goes to shouts and slammed doors, and nothing
   downstream needs the gain (ASR and speaker models normalise level). If
   the app's live audio is unusably quiet, lower `MIC_BIT_SHIFT` to 15
   (doubles it) and halve every level threshold in the table below.
4. Pair in the Omi app as usual — the device advertises the standard OMI
   service; nothing app-side knows or cares that the mics changed.
5. Calibrate the own-voice gate (next section). Until you do, the device
   tier is running on guessed thresholds.

## Own-voice gate and the device-tier new-voice detector

Every 100 ms block of the selected mono stream is high-passed at 110 Hz,
run through a five-band octave filterbank (250 Hz – 4 kHz), and classified
as **silence**, **own** (the wearer) or **other**. Segments of *other*
speech are averaged into a coarse spectral profile and compared with a
four-entry gallery of voices heard in the last ten minutes; a segment that
matches none is a **candidate**, which arms the burst hold-off
(`docs/SPEC.md` §2.1). Pure logic lives in `voice_dsp.h` and
`voice_logic.h`; `test/test_voice.cpp` drives it with synthetic voices.

The gate is **level-first**. The wearer's mouth is ~25 cm from the pendant
and nobody else is closer than ~50 cm, so own voice is 6–12 dB louder than
any partner at equal effort. That is the one cue that does not depend on
the enclosure. A spectral-tilt veto (own voice reaches the chest off-axis
and through the body, so it is darker) is there too, but it *does* depend
on the enclosure and ships disabled.

**Calibration**, from the `VOICE:` line that prints every 10 s:

```
VOICE: other level=14 floor=1.6 tilt=+2.3 bands=41/44/40/37/33 known=1
```

1. Sit in a quiet room. `floor` should settle near the room level (1–3).
2. Talk normally for a minute, reading the line. Note `level` and `tilt`.
3. Have someone talk to you from 1 m, then from 0.5 m. Note both again.
4. Set `VOICE_OWN_LEVEL` between your level and the 0.5 m partner's. If the
   two overlap, set `VOICE_OWN_TILT_MIN_DB` between your tilt and theirs —
   that is what the veto is for.
5. Wear it for a day. Every scored segment prints
   `VOICE: segment matched slot N dist=X` or `VOICE: new-voice candidate
   dist=X`, and every candidate that armed a hold-off is a `candidate` row
   in `events.csv` with its score. Same-speaker distances cluster low,
   genuinely new voices high; put `NOVELTY_DIST_DB` between the clusters.
   The shipped 3.0 dB comes from synthetic voices and is a starting point,
   not a measurement.

What it cannot do: tell two similar voices apart, or notice a new voice
that only ever speaks in bursts shorter than `NOVELTY_MIN_BLOCKS` (1.5 s).
It is the fallback tier; the phone and server verdicts override it.

## Thermal

`thermal.h` samples the S3 die every 20 s with the battery check. **It does
not gate charging** — the XIAO's BQ25101 has no thermistor and no reachable
enable pin, so firmware cannot stop it charging a hot cell. What it does:
above `THERMAL_WARM_C` bursts pause; above `THERMAL_HOT_C` a running video
session stops and the CPU drops to its floor; each transition is a
`thermal` row in `events.csv`. The die runs 10–20 °C above the cell under
load; measure that offset once with a thermocouple on the cell and move the
thresholds so `WARM` lands at the cell's 45 °C charge limit.

## Tuning knobs (`src/config.h`)

| Define | Default | Meaning |
|---|---|---|
| `MIC_BIT_SHIFT` | 16 | 32-bit slot → int16; smaller = louder (15 ≈ 2x). Rescale every level below if you change it |
| `MIC_HIGHPASS_HZ` | 110 | First-order high-pass on every channel; 0 disables |
| `MIC_LAPEL_FITTED` | 0 | Set 1 when the lapel pod exists |
| `MIC_LAPEL_PRESENT_LEVEL` | 10 | Level that marks the lapel live |
| `MIC_LAPEL_HOLD_MS` | 5000 | Lapel stays selected this long after signal |
| `MIC_SWITCH_RATIO_*`, `MIC_SWITCH_BLOCKS` | 3/2, 3 | Case-mic switch hysteresis |
| `MIC_SWITCH_SILENCE_LEVEL` | 8 | Changeover only below this level (no mid-word splices) |
| `MIC_STATS_INTERVAL_MS` | 10000 | Cadence of the `MIC:` and `VOICE:` log lines |
| `VOICE_SPEECH_RATIO`, `VOICE_SPEECH_MIN_LEVEL` | 3, 6 | Speech: above floor × ratio and above the absolute floor |
| `VOICE_OWN_LEVEL` | 30 | Own voice above this level — **calibrate** |
| `VOICE_OWN_TILT_MIN_DB` | off | Own voice also needs tilt ≥ this; set from the `VOICE:` line |
| `VOICE_HANGOVER_MS` | 300 | A state outlives its last qualifying block by this |
| `NOVELTY_MIN_BLOCKS` | 15 | 1.5 s of *other* speech before a segment is scored |
| `NOVELTY_SEGMENT_GAP_MS` | 1500 | Silence this long ends a segment |
| `NOVELTY_DIST_DB` | 3.0 | Band-profile distance that counts as a new voice — **calibrate** |
| `NOVELTY_FORGET_MS` | 10 min | A voice unheard this long is new again |
| `THERMAL_WARM_C`, `THERMAL_HOT_C`, `THERMAL_HYST_C` | 60, 70, 5 | Die temperature thresholds (see Thermal) |
| `CAMERA_FRAME_SIZE` | `FRAMESIZE_UXGA` | 1600×1200 |
| `CAMERA_JPEG_QUALITY` | 10 | 10–63, **lower is better** |
| `VIDEO_FRAME_INTERVAL_MS` | 30000 | One frame per 30s |

### Choosing frame interval and quality

Storage is not the constraint — the SPI SD interface and the camera's grab
time are. Because the recorder runs in its own task, a slow grab costs frame
latency, never audio. Rough figures at quality 10:

| Frame size | Per frame | At 1/30s | 32GB holds |
|---|---|---|---|
| UXGA 1600×1200 | ~250 kB | ~30 MB/h | ~1000 h |
| SXGA 1280×1024 | ~150 kB | ~18 MB/h | ~1700 h |
| VGA 640×480 | ~50 kB | ~6 MB/h | ~5000 h |

Shortening the interval scales linearly: UXGA at one frame per 5s is still
only ~180 MB/h. The practical floor is about 1–2s between UXGA frames, where
grab plus write starts to saturate the card. If you ever want true motion
video, drop to VGA and use [ESP32-CAM_MJPEG2SD](https://github.com/s60SC/ESP32-CAM_MJPEG2SD),
which writes a real AVI container — don't push this frame-per-file scheme
past a few frames per second.
