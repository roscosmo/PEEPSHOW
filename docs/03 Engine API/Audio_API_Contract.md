# Audio API Contract

This document defines the Engine-facing audio API for PeepOS packages and game-development tools.

The audio API exposes symbolic package primitives. It does not expose SAI, DMA, LPTIM, GPIO, amplifier pins, mixer buffers, decoder internals, storage paths, or hardware timing callbacks.

Related:

- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Package_Asset_Loading_API_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Audio_Contract]]
- [[Power_and_Sleep_Policy]]
- [[Package_Blob_Format_Contract]]

---

## Purpose

Audio is a creative package primitive.

PeepOS does not require packages to remain semantically complete when muted. A package may be silent, audio-enhanced, audio-centric, rhythm-based, musical, toy-like, diagnostic, or any other bounded use of the audio primitives.

PeepOS guarantees only:

- safe symbolic audio submission
- validated package audio assets and BBB patterns
- bounded playback behavior
- system-owned output policy
- fault isolation and diagnostics

PeepOS does not guarantee that audio is perceptible to the user. The user may mute the device, use the device in a quiet environment, or experience audio suppression due to system policy.

The selected target profile also determines which symbolic output classes exist. HW6 grants speaker music/SFX behavior but has no PAM/piezo output, so `audio.bbb` is false. Packages requiring BBB must provide an admitted fallback or fail HW6 compatibility validation. Retaining BBB API definitions for other targets does not grant hardware support to HW6.

---

## Ownership Boundary

The Platform owns:

- speaker hardware
- BBB/piezo hardware where the target grants it
- SAI, DMA, target-granted procedural-output peripherals, GPIO, amplifier shutdown, and clock interaction
- mixer and decoder implementation
- output power state
- global mute/silence policy
- audio quiesce/resume for sleep and mode transitions
- audio fault recovery and quarantine

The Engine owns:

- package audio cue declarations
- cue ID resolution
- package audio context validation
- symbolic music/SFX/BBB APIs
- volume and mute intent admission
- audio timeline events for diagnostics and digital twin replay
- package-facing behavior when physical output is muted or suppressed

Packages own:

- authored music cues
- authored SFX cues
- authored BBB patterns
- bounded procedural BBB requests
- package-local cue priorities, groups, and defaults
- gameplay use of audio as creative content

Packages do not own physical output policy.

---

## Core Rules

- Packages use symbolic audio cue IDs and bounded procedural BBB requests.
- Package code must not control SAI, DMA, LPTIM, GPIO, `SD_MODE`, amplifier state, audio clocks, mixer buffers, or decoder state.
- Package code must not stream audio from FAT, host paths, or arbitrary filesystem paths during active runtime.
- Audio assets and BBB patterns must be package-contained and validated before install/runtime use.
- Physical audio output may be muted, suppressed, faded, ducked, stopped, or quarantined by PeepOS policy.
- Audio-centric gameplay is allowed.
- Muted or suppressed output is not a package validation failure.
- Low-level audio faults are Platform/Engine diagnostics, not normal package driver logic.
- Game timing may use Engine/runtime clocks or symbolic timeline events, not DMA buffer callbacks or hardware completion interrupts.

---

## Output Classes

PeepOS exposes two package-facing audio output classes:

| Class | Meaning |
|---|---|
| `speaker` | sampled music and SFX cues rendered through the Platform speaker path |
| `bbb` | bounded procedural beep/boop/buzz patterns rendered through the BBB/piezo path |

Speaker and BBB may be active at the same time when Platform policy permits.

BBB is not a second PCM engine. It is symbolic/procedural output.

---

## Capability Model

Canonical names live in [[PeepOS_Capability_Registry]].

Audio capabilities:

| Capability | Meaning |
|---|---|
| `audio.music` | package may declare and play symbolic music cues |
| `audio.sfx` | package may declare and play symbolic SFX cues |
| `audio.bbb` | package may declare BBB patterns and bounded procedural BBB output |
| `audio.volume_intent` | package may express package-local bus volume and mute intent |
| `audio.timeline` | package/twin diagnostics may observe symbolic cue timeline events |

Capability names must not include SAI, DMA, LPTIM, GPIO, pin names, amplifier names, or codec implementation names.

---

## Audio Profile Schema

Packages declare audio use as package data.

Conceptual schema:

```text
audio_profile:
  cues[]:
    cue_id
    cue_type
    asset_ref
    bus
    group
    priority
    default_volume
    loop_policy
    fade_policy
    ducking_policy
    max_duration_ms
    preload_policy
  bbb_patterns[]:
    pattern_id
    steps[]
    priority
    max_duration_ms
  bbb_melodies[]:
    melody_id
    source_format = rtttl
    source_ref
    compiled_pattern_ref
    priority
    max_duration_ms
  audio_contexts[]:
    context_id
    runtime_unit_refs[]
    active_cue_refs[]
    bbb_pattern_refs[]
    volume_defaults
    preload_refs[]
    power_behavior_hint
    diagnostic_label
```

Cue types:

| Cue Type | Meaning |
|---|---|
| `music` | long or looping sampled cue, one active music voice in the current Platform baseline pending HW6 validation |
| `sfx` | short sampled sound effect cue |
| `bbb_pattern` | authored procedural BBB pattern |
| `bbb_melody` | authored melody source compiled by tooling into a BBB pattern |
| `bbb_tone` | bounded procedural BBB tone request |
| `bbb_sweep` | bounded procedural BBB sweep request |

Rules:

- every cue ID must be package-local and stable.
- every cue asset reference must resolve at package validation time.
- cue groups and priorities must be declared, bounded, and deterministic.
- audio contexts must be tied to declared runtime units.
- active audio may raise the Platform power floor, but packages do not choose sleep class.
- physical output suppression must not corrupt package state.

---

## Speaker Cue Semantics

Speaker cues represent sampled audio assets prepared by the asset pipeline.

Package-facing operations:

```text
music.play(cue_id, loop_policy, fade_ms, volume_intent)
music.stop(fade_ms)
music.pause()
music.resume()
sfx.play(cue_id, priority, volume_intent)
sfx.stop_group(group_id)
```

HW5 baseline expectations from [[Audio_Contract]]:

- mono output
- 16 kHz baseline sample rate
- bounded ADPCM package assets
- one music voice
- five SFX voices
- deterministic priority/preemption
- bounded fade and ducking behavior

The final package-facing API may expose these as handles, commands, events, or declarative actions, but it must preserve symbolic cue semantics.

---

## BBB Semantics

BBB output is procedural symbolic audio.

Package-facing operations:

```text
bbb.play_pattern(pattern_id, priority)
bbb.stop_group(group_id)
bbb.tone(freq_hz, duration_ms, envelope)
bbb.sweep(start_freq_hz, end_freq_hz, duration_ms, curve)
```

Allowed BBB pattern steps:

- tone
- gap
- sweep
- repeat group with bounded count
- drive/envelope change from an approved set

Rules:

- BBB duration must be bounded.
- BBB step count must be bounded.
- frequency range must be target-profile bounded.
- unsupported curves/envelopes fail validation.
- BBB patterns are package assets or approved built-in system patterns.
- BBB output may be muted or suppressed by PeepOS policy like other audio output.

### BBB Melody Authoring

BBB melody authoring is a tooling feature, not a runtime parser.

V1 melody source format is RTTTL/Nokia ringtone text.

```text
pet_happy:d=8,o=5,b=140:c,e,g,2c6,p,g
```

Tooling must compile RTTTL source into validated BBB pattern steps before package export.

```text
RTTTL source
  -> melody parser
  -> normalized note/rest events
  -> target-profile validation
  -> compiled BBB pattern asset
  -> runtime bbb.play_pattern(pattern_id, priority)
```

Runtime firmware must not parse RTTTL text.

Supported v1 RTTTL subset:

- monophonic notes `a` through `g`
- optional sharp notes with `#`
- pause/rest with `p`
- durations `1`, `2`, `4`, `8`, `16`, and `32`
- dotted notes where target tooling supports deterministic expansion
- bounded octave range from the selected target profile
- bounded tempo range from the selected target profile

Rules:

- RTTTL source is an authoring input or source-side package asset only.
- compiled packages contain validated BBB pattern data, not required runtime RTTTL source.
- every note must map to a target-profile-valid frequency.
- every duration must map to a target-profile-valid duration.
- total melody duration, step count, repeat count, envelope, drive class, and priority must fit target-profile limits.
- unsupported RTTTL features, out-of-range notes, unbounded loops, chords, polyphony, or runtime tempo mutation fail validation.
- melody playback uses normal BBB priority, preemption, mute, stop/drain, and sleep-quiesce policy.

---

## Volume And Mute Intent

Packages may express package-local volume or mute intent.

Audio buses:

- `master`
- `music`
- `sfx`
- `bbb`

Package-facing operations:

```text
audio.set_bus_volume_intent(bus, value_0_100)
audio.set_mute_intent(bus, enabled)
```

Rules:

- package volume/mute intent is not Platform settings mutation.
- PeepOS global mute, quiet policy, diagnostics policy, and fault policy override package intent.
- package settings may store package-local audio preferences through [[Package_Save_Settings_API_Contract]].
- packages must not infer that physical sound was produced from intent success.

---

## Audio Timeline Events

The Engine may expose symbolic audio timeline events for diagnostics, host replay, and package logic where appropriate.

Examples:

```text
cue_started(cue_id)
cue_stopped(cue_id, reason)
cue_looped(cue_id)
cue_marker(cue_id, marker_id)
bbb_pattern_complete(pattern_id)
audio_suppressed(reason)
```

Rules:

- timeline events are symbolic Engine events, not hardware interrupts.
- muted playback may still produce symbolic timeline events if the Engine can preserve the timeline safely.
- packages must not depend on DMA callbacks, buffer refill timing, SAI completion, or LPTIM interrupts.
- marker timing must be deterministic enough for the selected runtime profile before tools allow package use.

---

## Runtime Class Rules

| Runtime Class | Audio Behavior |
|---|---|
| `LP_GRAPH` | symbolic cue triggers and short bounded BBB/SFX only; no unbounded active playback requirement |
| `LP_MODULE` | music/SFX/BBB allowed when declared and bounded by context |
| `RT_SCENE` | active music/SFX/BBB allowed while realtime activity remains valid |

Waiting visuals do not execute arbitrary package audio logic. Platform may stop, fade, or suppress audio when a reactive host yields or when system policy requires it.

Automatic input locking applies when enabled by the package. Audio may count as meaningful active work only when declared and accepted by runtime policy; passive looping audio cannot create unbounded lock deferral.

---

## Tool-Time Validation

Tooling must validate audio before package compilation/export.

Reject:

- unresolved cue IDs or asset refs.
- unsupported sample format.
- unsupported cue type.
- audio asset larger than target profile budget.
- music/SFX voice assumptions beyond target profile limits.
- unbounded loops where the runtime context does not allow them.
- BBB frequency, duration, step count, repeat count, curve, or envelope outside target bounds.
- cue priority/group definitions that are ambiguous or unbounded.
- package references to SAI, DMA, LPTIM, GPIO, `SD_MODE`, amplifier state, mixer buffers, decoder internals, FAT paths, host paths, or hardware callbacks.
- runtime unit audio context that conflicts with power/runtime class rules.

Authoring tools should explain failures in PeepOS terms, such as:

```text
BBB pattern has 96 steps; this target allows 32.
```

They should not expose low-level audio hardware names to normal game authors.

---

## Runtime Suppression And Fault Handling

Physical output may be unavailable because of:

- user mute
- quiet policy
- low-power transition
- installer/storage policy
- audio owner fault
- speaker path quarantine
- BBB path quarantine

Normal package audio APIs remain symbolic. Platform/Engine decides whether to keep a silent timeline, suppress the cue, stop active cues, or apply lifecycle policy.

Audio faults are diagnostics and Platform health events. Package gameplay code must not handle SAI, DMA, amp, LPTIM, or decoder fault causes.

---

## Digital Twin Requirements

The digital twin must use the same audio contract as the hardware runtime.

The twin may:

- play host audio.
- validate silently.
- record an audio event timeline.
- compare deterministic cue timelines in replay tests.
- inject audio suppression or fault events for lifecycle/diagnostic validation.

Rules:

- host audio playback is not required for deterministic tests.
- BBB may be rendered audibly, visualized, or timeline-only in the host.
- cue admission, priority, preemption, loops, fades, markers, and suppression must match the Engine contract.
- hardware waveforms and DMA timing are not modeled.
- twin audio evidence is not HW5 audio bring-up evidence.

---

## Validation Cases

1. package music cue resolves to a valid package asset and plays symbolically.
2. SFX priority/preemption is deterministic within the target voice limit.
3. BBB pattern validates frequency, duration, step count, repeat count, and envelope bounds.
4. package volume/mute intent is overridden by global mute without corrupting package state.
5. package has no access to SAI, DMA, LPTIM, GPIO, `SD_MODE`, amplifier state, mixer buffers, or decoder internals.
6. package audio path performs no FileX/FAT or host-path streaming during active runtime.
7. audio suppression does not become a low-level driver error visible to normal package gameplay code.
8. symbolic timeline replay is deterministic in the digital twin.
9. audio fault injection records diagnostics and applies Platform/Engine policy without treating the injected fault as hardware bring-up evidence.
10. audio-centric package behavior is allowed when it remains within bounded package/runtime rules.
