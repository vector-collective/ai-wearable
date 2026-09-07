# System spec — speaker-aware pendant

Rev 1, September 2026. Supersedes the memory-recall framing; that exercise
moves to a separate project fed by this one's transcripts.

## 1. Purpose

A chest-worn pendant that captures the wearer's conversations, learns who the
wearer is talking to, and feeds a pipeline that reports on the wearer's own
conduct and mood, per relationship and over time. Everything the wearer has to
act on lands in an approval queue; nothing is written to a calendar, contact
list, or task list without a yes.

Concretely:

1. **Speaker identity.** Voices are clustered into IDs (`S001`, `S002`…).
   The wearer labels them later. Merges are proposed, never silent.
2. **New-voice photo burst.** When a voice not in the gallery is heard, the
   camera takes a photo every 3 s for 30 s, then powers down. Every photo is
   timestamped so it lands on the same timeline as the audio. The wearer later
   tags a photo with the speaker ID — manually, by design.
3. **Per-relationship trends.** Tone, warmth, brevity, interruptions, per
   speaker, over any window.
4. **Self-analysis.** Mood; how the wearer treats people; statements of the
   wearer's with more than one plausible reading that the other party never
   reflected back (*misconstrued and unconfirmed*); moments where the other
   party's reply shows they took it differently (*confirmed misunderstood*);
   periodic mood check-ins.
5. **Action-item extraction.** To-dos, contacts, calendar items, tasks and
   project updates mentioned in conversation → approval queue.
6. **Activity log.** What the wearer was doing and when, inferred from the
   sensors; gaps batched into one end-of-day prompt.

### Non-goals

- Automatic face recognition. Face-to-voice linking is manual.
- Real-time coaching or alerts. The pendant is silent in the moment.
- Distribution. Personal use, one wearer. If that ever changes, the biometric
  section below has to be redone per region before anything ships.

## 2. Architecture

The ESP32-S3 cannot run diarization, transcription, or any language model.
So the split is fixed:

```
 pendant (ESP32-S3)          phone (BLE peer)              home server
 ─────────────────           ────────────────              ───────────
 3× INMP441 → Opus  ──BLE──▶ relay + light diarizer ──WiFi──▶ full pipeline
 camera → SD (JPEG)          RTT probe to server           diarize / embed
 events.csv timeline         verdicts back to pendant      transcribe / LLM
 burst policy + hold-off     time sync on connect          approval queue
 local fallback trigger                                     trends, reports
```

### 2.1 Hybrid trigger, and the latency policy

The new-voice decision has three possible authorities, in descending accuracy:

| Tier | Where | Model | When it is used |
|---|---|---|---|
| server | home server | full speaker embedding + clustering | phone reachable, RTT below threshold |
| phone | phone app | light embedding model | server unreachable or slow |
| device | pendant | own-voice gate + spectral novelty | BLE disconnected, or no verdict in time |

**The device does not measure network latency. The hold-off timer *is* the
latency policy.** When the device's own detector sees a candidate new voice it
arms a hold-off (`BURST_HOLDOFF_MS`, 5 s). If a remote verdict — from either
upper tier, relayed through the phone — arrives inside the hold-off, that
verdict wins: *new* fires the burst, *known* cancels it. If nothing arrives,
the device fires on its own authority. A slow server, a dead phone, and a
disconnected BLE link all degrade to the same path without any of them having
to be detected explicitly.

The phone decides between server and phone tiers by probing server RTT; the
threshold lives on the phone. Remote tiers may also send an *unsolicited* new
verdict (the device's detector missed it); that fires directly, subject to the
same suppression.

### 2.2 Burst suppression

Diarization clusters wobble. Without suppression the same person triggers
repeatedly as their cluster splits and re-forms. Rules, all in `burst_logic.h`:

- **quiet mode** — no bursts, ever, until cleared
- **daily cap** — `BURST_DAILY_CAP` (20) in a rolling 24 h
- **per-speaker suppression** — once a burst fires for a hint, that hint is
  suppressed for `BURST_SUPPRESS_MS` (30 min)
- **minimum gap** — `BURST_MIN_GAP_MS` (60 s) between any two bursts,
  because the device tier has no speaker hint to key suppression on

### 2.3 Photos go to SD, not over BLE

A JPEG is 20–50 KB; the BLE photo path moves 200 bytes per chunk. Ten photos in
30 s is 250–500 KB and BLE would still be uploading the first when the burst
ended. Bursts write to `/rec/bursts/` via the recorder task, the same path that
already writes video-session frames. The BLE photo path stays for the app's
single-shot use.

### 2.4 Time

There is no RTC. Every event carries `boot_id` and `millis`; the phone writes
Unix epoch milliseconds to `TIME_SYNC` on every connect, and the device logs
that as a `sync` event. Post-processing has, for every boot, a monotonic clock
and one or more anchors, and interpolates. ESP32 drift is tens of ppm — under a
second per session. Events before the first mount buffer in RAM and flush on
mount, so a sync received before the card is up is not lost.

`boot_id` is an NVS counter incremented on every boot. It is what makes
`millis` meaningful across power cycles.

## 3. Protocols

### 3.1 BLE additions (service `19B10000-…`)

| UUID | Dir | Payload |
|---|---|---|
| `19B10007` CAPTURE_CTRL | write | op byte, then per-op fields (below) |
| `19B10008` TIME_SYNC | write | `u64 LE` epoch milliseconds |

CAPTURE_CTRL ops, all little-endian:

| op | name | fields | effect |
|---|---|---|---|
| `0x01` | VERDICT_NEW | `u8 src, u32 hint` | remote says a new voice: fire (subject to §2.2), cancel hold-off |
| `0x02` | VERDICT_KNOWN | `u8 src, u32 hint` | remote says known voice: cancel hold-off, no burst |
| `0x03` | BURST_FORCE | `u8 count, u16 interval_ms` | manual burst; bypasses everything except quiet mode |
| `0x04` | QUIET_ON | — | stop bursts (and, once wired, mic feed) |
| `0x05` | QUIET_OFF | — | |
| `0x06` | CANCEL | — | abort an in-progress burst |

`src`: 0 device, 1 phone, 2 server. `hint`: the remote's cluster ID, opaque to
the device, used only as a suppression key.

### 3.2 SD layout

```
/rec/events.csv                     timeline spine, append-only
/rec/S0001/seg01/audio.wav          video sessions (unchanged)
/rec/S0001/seg01/f000000.jpg
/rec/bursts/B<boot>_<millis>_<i>.jpg
```

### 3.3 events.csv

```
boot_id,millis,epoch_ms,type,detail
```

`epoch_ms` is 0 when unsynced. Types: `boot`, `sync`, `session_start`,
`session_stop`, `burst_start` (detail: `src,hint,count,interval`),
`burst_frame` (detail: path), `burst_end`, `quiet_on`, `quiet_off`. This one
file is what the pipeline reads first; everything else is referenced from it.

## 4. Pipeline (server, `pipeline/`)

Stages, each idempotent over `events.csv` + the referenced files:

1. **ingest** — pull SD contents (or the phone's relay cache), resolve every
   event to epoch time using the sync anchors per boot
2. **diarize + embed** — speaker segments and embeddings; gallery of clusters
   with first/last seen, wearer's own voice enrolled explicitly on first run
3. **link** — bursts get the set of speaker IDs active in ±30 s; exactly one
   non-wearer speaker → strong candidate, else the wearer disambiguates
4. **transcribe** — with speaker labels
5. **analyse** — one LLM pass per conversation against a fixed schema:
   wearer tone, other tone, ambiguous-and-unconfirmed statements,
   confirmed-misunderstood moments, action items, mentions
   (contacts / dates / tasks / projects)
6. **aggregate** — per-speaker trends over selectable windows; wearer mood
   over time; activity timeline from pendant + phone (GPS, motion, screen)
7. **queue** — approval items: action items, cluster merges, burst → speaker
   tags, activity-log gaps. Approve / edit / reject; approved items are the
   only thing that writes outward

Interim home: this repo, `pipeline/`. Target: the wearer's home server. The
phone becomes a relay plus the light-tier diarizer.

## 5. Privacy and legal design

Personal use, one wearer, no distribution. Texas one-party consent covers the
audio. Voiceprints and face photos of third parties are biometric identifiers
under Texas CUBI and would need informed consent *if* captured for a
commercial purpose — which this is not, and the org repo is not a signal that
it is. The design still carries the mitigations, because they are cheap and
because the legal picture changes the moment the wearer crosses into a
two-party-consent state:

- all biometric processing on the wearer's own hardware; no cloud matching
- **quiet mode**: button gesture or phone toggle, stops camera and (once
  wired) mic — bathrooms, medical settings, anyone who asks
- **per-person purge**: "forget S007" deletes voiceprint, burst photos, and
  every transcript segment attributed to them, in one action
- face-to-voice linking stays manual
- retention: raw audio and bursts age out on a schedule; derived summaries
  persist. Schedule TBD.

## 6. Decisions log

| Date | Decision |
|---|---|
| 2026-09 | Drop memory recall from this project |
| 2026-09 | Hybrid trigger, hold-off as the latency policy |
| 2026-09 | Bursts to SD, not BLE |
| 2026-09 | Time via phone sync + boot_id/millis, no RTC part |
| 2026-09 | Pipeline lives in `pipeline/` for now, target home server |
| 2026-09 | Personal use; not a product; biometric mitigations kept anyway |

## 7. Open

- Device-tier new-voice detector: needs the own-voice gate first
- Quiet mode should also stop the mic feed; today it only gates bursts
- Retention schedule
- Whether the phone-tier model is worth building, or whether phone → server
  with the device fallback is enough in practice
