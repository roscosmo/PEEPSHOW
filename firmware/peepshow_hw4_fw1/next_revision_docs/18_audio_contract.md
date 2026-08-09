# Audio Contract

This document defines audio ownership, playback model, and power coordination.

---

## Ownership

- `thAudio` is the sole owner of audio peripheral handles, DMA, and amp control.
- Other threads submit commands through `qAudioCmd`.
- Clock profile requests flow through `thPower` only.

---

## Audio FSM

Use states from `05_subsystem_state_machines.md`:
- `AUDIO_OFF`
- `AUDIO_UI_ONLY`
- `AUDIO_STREAMING`
- `AUDIO_SUSPENDED`
- `AUDIO_ERROR`

All commands must be state-validated.

---

## Data Source Rules

Audio runtime sources are limited to:
- installed/raw package assets
- built-in system assets

Audio must not stream from FAT during active runtime playback.

---

## Playback Model

Required support:
- short UI/system cues
- longer streaming or loop playback

Rules:
- bounded buffering only
- deterministic refill path
- no blocking behavior in ISR
- no runtime heap dependency

---

## DMA Refill Discipline

ISR behavior:
- set flags and return

Thread behavior:
- refill active DMA window
- detect underrun
- report bounded error events

Underrun during steady-state playback is a defect.

---

## Power Coordination

- audio owner publishes active/inactive state events
- power owner applies and releases profile floor when needed
- before deep sleep: stop DMA, park peripheral, disable amp safely

---

## Validation Cases

1. cue playback latency and correctness
2. long playback stability and underrun checks
3. rapid start/stop transition robustness
4. quiesce/resume behavior across mode transitions
5. installer mode isolation behavior

