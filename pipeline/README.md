# Pipeline

The server side of the speaker-aware pendant: everything the ESP32-S3
cannot do. Spec is [`docs/SPEC.md`](../docs/SPEC.md), section 4.

**Status: scaffold.** Nothing here runs yet. The firmware side it consumes —
`events.csv`, timestamped bursts, the capture-control and time-sync
characteristics — landed first so the data exists to build against.

## What it reads

```
/rec/events.csv                        the timeline spine; read this first
/rec/S0001/seg01/audio.wav             video sessions
/rec/S0001/seg01/f000000.jpg
/rec/bursts/B<boot>_<millis>_<i>.jpg   new-voice photo bursts
```

`events.csv` rows are `boot_id,millis,epoch_ms,type,detail`. Every boot has
one or more `sync` rows carrying `millis=… epoch=…`; resolve any event's wall
time by interpolating from the nearest anchor in the same boot. `epoch_ms` of
0 means unsynced at the time — not missing.

## Stages

| # | Stage | In | Out |
|---|---|---|---|
| 1 | ingest | SD contents or the phone's relay cache | events resolved to epoch time, per boot |
| 2 | diarize + embed | audio | speaker segments, embeddings, a gallery of clusters; the wearer enrolled explicitly |
| 3 | link | bursts + segments | each burst gets the speaker IDs active ±30 s; one non-wearer → strong candidate |
| 4 | transcribe | audio + segments | speaker-labelled transcript |
| 5 | analyse | transcript | per-conversation schema: tones, ambiguous-unconfirmed, confirmed-misunderstood, action items, mentions |
| 6 | aggregate | analyses | per-speaker trends over windows; wearer mood; activity timeline with phone signals |
| 7 | queue | everything | approval items — action items, cluster merges, burst→speaker tags, activity gaps |

Each stage is idempotent over its inputs so a re-run after a model change
only redoes what changed.

## Intended stack

Python. pyannote or WeSpeaker for diarization and embeddings; faster-whisper
for transcription; a local LLM for stage 5 against a fixed JSON schema;
SQLite for the gallery, queue and aggregates. Home server target; the phone
becomes a BLE relay plus the light-tier diarizer that sends verdicts back.

## Privacy defaults

Per-person purge is a first-class operation: "forget S007" removes the
voiceprint, every burst photo linked to it, and every transcript segment
attributed to it. Raw audio and bursts age out on a schedule; derived
summaries persist. All processing on the wearer's own hardware.
