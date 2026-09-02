# Audio Contract

This document defines PeepShow audio ownership, playback architecture, state machines, and power coordination across target profiles.

Audio is owned by Platform. Engine and Reference Game code request only symbolic behavior granted by the selected target profile; they must not control SAI, DMA, LPTIM, GPIO, or amplifier pins directly.

Implementation status on HW6 includes one target-proven STATE sampled-SFX
path. The `.egg` format carries sampled-audio asset, ADPCM-bank, and cue
chunks; STATE actions can request a symbolic cue through `qAudioCmd`; and
`thAudio` owns bounded package-backed ADPCM decode, fixed-ring SAI DMA playback,
MAX98357A shutdown, clock-intent release, and return to STOP2. The same cue is
target-proven both before and after a real STOP2 cycle. Post-STOP restoration is power-owned: `thPower`
restores and verifies the voltage scale, re-arms the PLL2P epoch, hands the SAI
kernel mux back to PLL2P, and grants `SAI_AUDIO_ACTIVE` before playback starts.
Music, sustained playback, mixing, preemption, fades, and measured audio energy
remain open. Package SFX streaming has fixed buffers and underrun telemetry, but
still requires target evidence with a cue longer than the two DMA halves.

## Target Applicability

- HW6 retains the SAI1/MAX98357A speaker path.
- HW6 removes the PAM8904/piezo path and physical `PB2` output. HW6 target profiles must set `audio.bbb_supported = false` and block capability `audio.bbb`.
- LPTIM1 remains on HW6 as an internal LPBAM timing resource; it is not owned by `thAudio` merely because HW5 used `LPTIM1_CH1` for BBB output.
- BBB sections below describe the retired HW5 implementation and any future target that explicitly grants `audio.bbb`; they do not apply to HW6.

## Hardware Paths

| Path | Hardware | MCU Signals | Purpose | Owner |
|---|---|---|---|---|
| Speaker | `MAX98357AETE+T` into 1 W 20 mm speaker | `SAI1_A`: `PA8` SCK, `PB9` FS, `PA10` SD | music and sampled SFX | `thAudio` |
| Speaker shutdown | MAX98357A `SD` pin | `PC9` `SD_MODE` | speaker amp shutdown/enable | `thAudio` |
| BBB / piezo (HW5 only) | `PAM8904EGPR` into piezo buzzer | `PB2` `BUZZ` / `LPTIM1_CH1` | beeps, boops, buzzes, procedural tones | `thAudio` when target grants `audio.bbb` |

`SD_MODE` low places the MAX98357A in shutdown. `SD_MODE` high enables the speaker amp.

The PAM8904 path auto-shuts down when no DIN signal is present.

## Ownership

- `thAudio` is the sole owner of audio playback state, SAI1 data-path operation, audio DMA, `SD_MODE`, mixer state, decoder state, and audio fault recovery.
- `thPower` is the sole owner of PWR voltage-scale transitions, RCC/PLL2 configuration, the SAI kernel-clock mux, and SAI clock/reset gating. `thAudio` requests the symbolic `SAI_AUDIO_ACTIVE` capability and never changes those power/clock registers directly.
- On a target that grants `audio.bbb`, `thAudio` also owns that target's procedural-output peripheral and signal. HW6 grants no such path.
- Other threads submit commands through `qAudioCmd`.
- Clock profile requests flow through `thPower` only.
- Speaker and BBB paths may play simultaneously only on a target profile that grants both.

## Audio Architecture

A target with both speaker and BBB capabilities has two output classes under one owner. HW6 instantiates only the speaker branch:

```text
thAudio
|-- Speaker path: sampled/mixed audio over SAI1/I2S to MAX98357A
`-- BBB path: procedural symbolic audio over LPTIM1/BUZZ to PAM8904
```

Speaker path:

- 16 kHz mono output
- 16-bit PCM DMA output
- 4-bit IMA ADPCM assets, mono, 16 kHz
- admitted `SEQUENCE_SCENE` and `PROGRAM_SCENE` execution may use exactly 1 music voice plus 5 SFX voices
- `STATE_SCENE` execution may use bounded SFX bursts only; music is rejected or deferred to a realtime/sustained-audio grant
- music/SFX mixing to mono PCM where the active target profile grants sustained audio
- volume, mute, fade, ducking, priority, and preemption

### Initial HW6 STATE SFX Milestone

The first package-facing audio milestone is deliberately smaller than the full
mixer contract:

- one package-backed streamed sampled-SFX voice;
- source WAV converted by host tooling to mono 16 kHz 4-bit IMA ADPCM;
- complete compressed payload validated before playback, with ADPCM retained in
  the installed package blob and decoded into fixed PCM DMA halves as needed;
- one symbolic `AUDIO_PLAY_SFX(id, priority, volume)` request from a STATE
  action through the Engine-to-`thAudio` queue;
- no music, runtime FAT access, arbitrary procedural audio, or HW6 BBB support;
- deterministic completion, amplifier shutdown, clock-intent release, and
  return to reactive STOP2 admission after the burst drains.

Peep Studio owns source import, deterministic conversion, package metadata,
audition, and compatibility diagnostics. It never controls SAI, DMA,
`SD_MODE`, clocks, or playback buffers.

Host/package status as of service API 21: implemented and covered by
deterministic compiler/parser/preview tests. The optional PKG1 audio asset,
ADPCM bank, and cue chunks plus symbolic `play_sfx` action are available to
Peep Studio. HW6 loader routing and one-voice `thAudio` playback are target-
proven for a streamed multi-second cue, including audible output across natural
STOP2 cycles, deterministic drain, and clock release. This grants only the
one-voice `audio.sampled_sfx` subset described here, not music or the future
mixer.

The audio lifetime boundary is the active package, not an individual STATE
scene. A package-global `play_sfx` action may be committed with a successful
same-package scene replacement and continue while the destination scene is
active. Package suspension, exit to shell, replacement, or unmount must stop
and drain package-owned voices before their source bytes can become invalid.
PeepOS-owned shell and package-loading sounds use OS-owned assets and are not
package voices.

Current FW0 bring-up bounds are:

- source WAV: uncompressed mono or stereo PCM, 8/16/24/32-bit, 8..96 kHz;
- compiled output: mono 16 kHz 4-bit IMA ADPCM in independent 256-sample blocks;
- maximum 32 sampled-SFX assets, 64 cues, and 4 MiB compiled ADPCM bank;
- exactly one admitted STATE SFX voice, with two fixed PCM DMA halves refilled
  by `thAudio` from a resident package prefix or fixed 4 KiB package windows;
- packages at or below `65536` bytes retain their complete existing cache
  path. Larger packages retain the leading `65536` bytes and may expose only
  a final ADPCM bank through package-backed audio windows;
- `thStorage` owns each package-window read, while `thAudio` owns decode, PCM
  buffering, SAI, DMA, amplifier control, drain, and playback telemetry.

The approved active-package profile is `5 MiB` with a maximum `4 MiB` compiled
audio bank, approximately 8.3 minutes of this ADPCM format. There is no
separate duration limit per audio asset beyond the audio-bank and total-package
budgets. The compiler places `AUD1` and `ACU1` before the final `ADB1` bank so
the loader can validate all resident metadata before exposing package-backed
audio. The installer verifies the full package; the partial runtime loader
validates the resident table and metadata, and the decoder validates each ADPCM
block as it is read. Neither runtime path accesses FAT. Music, multi-voice
mixing, fades, ducking, priority, mute, and sustained-playback energy
characterization remain later milestones.

BBB path:

- built-in system BBB patterns
- package/game BBB pattern assets
- bounded procedural tones
- bounded procedural sweeps
- short melodies/sequences
- priority and preemption
- no PCM streaming

## Data Source Rules

Audio runtime sources are limited to:

- installed/raw package assets
- built-in system assets
- validated BBB pattern assets
- bounded procedural BBB requests

Rules:

- no FileX/FAT reads during active playback
- SFX ADPCM remains in the validated installed-package blob; `thAudio` decodes
  it into a fixed PCM DMA ring during playback
- music is fully preloaded or read from the installed package's raw flash asset
  region into a bounded ring buffer through `thStorage`
- ADPCM decode writes into bounded PCM buffers
- no runtime heap dependency in playback paths
- FileX/FAT is transport and staging only; music and dialogue never stream from
  the host-visible FAT volume during runtime

## Public Request Model

Engine and Reference Game may request:

- `AUDIO_PLAY_MUSIC(id, loop, fade_ms, volume)`
- `AUDIO_STOP_MUSIC(fade_ms)`
- `AUDIO_PAUSE_MUSIC()`
- `AUDIO_RESUME_MUSIC()`
- `AUDIO_PLAY_SFX(id, priority, volume)`
- `AUDIO_STOP_SFX(id_or_group)`
- `AUDIO_PLAY_BBB_PATTERN(pattern_id, priority)`
- `AUDIO_PLAY_BBB_TONE(freq_hz, duration_ms, drive_class, envelope)`
- `AUDIO_PLAY_BBB_SWEEP(start_freq_hz, end_freq_hz, duration_ms, curve)`
- `AUDIO_PLAY_BBB_SEQUENCE(step_table_id, priority)`
- `AUDIO_SET_VOLUME(bus, value)`
- `AUDIO_SET_MUTE(bus, enabled)`

Audio buses:

- `master`
- `music`
- `sfx`
- `bbb`

Platform may reject requests that exceed validated bounds. On HW6, music requests are valid only while the active scene has a realtime or future sustained-audio grant. A `STATE_SCENE` may request short SFX only; while the SFX is active `thAudio` may keep the MCU awake and hold `SAI_AUDIO_ACTIVE`, but it must release that clock intent when the burst drains so the system can return to its selected reactive waiting backend.

## BBB Pattern Model

BBB is procedural symbolic audio, not a second PCM audio engine.

BBB sequence assets should be tiny validated command lists, for example:

```text
tone 1200 Hz 40 ms
gap 20 ms
tone 1800 Hz 40 ms
gap 20 ms
sweep 2200 Hz -> 900 Hz 120 ms
```

Allowed BBB steps:

- tone
- gap
- sweep
- repeat group with bounded count
- drive/envelope change from a fixed allowed set

Rejected BBB requests:

- frequency outside allowed range
- duration outside allowed range
- unbounded repeat
- too many sequence steps
- unsupported curve/envelope
- request that conflicts with higher-priority BBB output

## Required Knobs

| Knob | Purpose |
|---|---|
| `KNOB_AUDIO_SAMPLE_RATE_HZ` | target PCM output rate, initially 16000 |
| `KNOB_AUDIO_MIXER_SFX_VOICES` | target-profile SFX voice count; HW5 used 5 and HW6 remains pending validation |
| `KNOB_AUDIO_PCM_DMA_FRAMES` | PCM DMA buffer frame count |
| `KNOB_AUDIO_MUSIC_RING_BYTES` | bounded music ring-buffer size |
| `KNOB_AUDIO_ADPCM_BLOCK_BYTES` | ADPCM decode block size |
| `KNOB_AUDIO_FADE_STEP_MS` | fade update cadence |
| `KNOB_AUDIO_DUCK_RELEASE_MS` | ducking release time |
| `KNOB_AUDIO_BBB_MAX_STEPS` | maximum BBB sequence steps |
| `KNOB_AUDIO_BBB_MAX_DURATION_MS` | maximum BBB request duration |
| `KNOB_AUDIO_RECOVERY_MAX_RETRIES` | bounded recovery attempts |

## Audio Owner FSM

| State | Meaning |
|---|---|
| `AUDIO_OFF` | audio hardware inactive, speaker amp shutdown, BBB idle |
| `AUDIO_INIT` | SAI/DMA/LPTIM/mixer/static buffers initialized |
| `AUDIO_IDLE` | ready but not playing |
| `AUDIO_ACTIVE` | speaker path and/or BBB path active |
| `AUDIO_SUSPENDING` | draining/stopping outputs for sleep or mode transition |
| `AUDIO_SUSPENDED` | audio quiesced |
| `AUDIO_RECOVERING` | bounded recovery after underrun/DMA/protocol fault |
| `AUDIO_ERROR` | major but non-fatal audio fault |

## Speaker Playback FSM

| State | Meaning |
|---|---|
| `SPK_OFF` | `SD_MODE` low, SAI/DMA inactive |
| `SPK_ENABLE` | `SD_MODE` high, amp enable/settle path |
| `SPK_IDLE` | speaker path ready, no active voice |
| `SPK_PRELOAD` | SFX or music metadata/buffer preflight |
| `SPK_BUFFERING` | ADPCM decode/ring-buffer fill before playback |
| `SPK_PLAYING` | mixer producing 16-bit PCM for DMA |
| `SPK_DRAINING` | fade/drain before stop or suspend |
| `SPK_PAUSED` | playback state retained but DMA/output stopped |
| `SPK_UNDERRUN` | mixer or ring buffer failed to provide data on time |
| `SPK_ERROR` | SAI/DMA/amp/mixer fault |

Speaker rules:

- realtime/sustained-audio operation grants exactly 1 music voice
- realtime/sustained-audio operation grants exactly 5 SFX voices unless the target profile reduces the count with evidence
- reactive operation grants bounded SFX bursts only and must not start music
- music and SFX mix into one mono PCM stream only where the active profile grants sustained audio
- SFX priority may preempt lower-priority SFX voices
- music ducking is allowed for important SFX only where music is admitted
- fades must be bounded and deterministic
- DMA ISR only signals; decode/mix/refill occurs in `thAudio`

## BBB / Piezo FSM

| State | Meaning |
|---|---|
| `BBB_OFF` | no BUZZ/LPTIM output |
| `BBB_READY` | BBB path ready for request |
| `BBB_PATTERN_LOAD` | built-in, asset, or procedural request validated and loaded |
| `BBB_PLAYING` | tone/sweep/sequence output active |
| `BBB_DRAINING` | final step or stop/preempt drain |
| `BBB_ERROR` | LPTIM/GPIO/pattern fault |

BBB rules:

- BBB may play concurrently with speaker output.
- BBB requests are symbolic/procedural, not PCM streams.
- BBB pattern assets must be validated by the asset pipeline.
- BBB output must stop cleanly so PAM8904 can auto-shutdown.

## Power Coordination

- Active speaker playback raises the power/performance floor and blocks deep sleep.
- Active BBB output may block deep sleep for the duration of the pattern.
- `thAudio` publishes active/inactive state to `thPower` and requests `SAI_AUDIO_ACTIVE` only while the speaker path genuinely needs the SAI kernel clock.
- Reactive SFX may hold `SAI_AUDIO_ACTIVE` only for the bounded burst/drain window, then must release it before PeepOS can return to STOP/LPBAM waiting behavior.
- Realtime audio may hold `SAI_AUDIO_ACTIVE` for the admitted realtime scene lifetime, subject to quiesce/suspend policy.
- FW0 runtime-admission evidence validates only the runtime-side `REALTIME_DEADLINE_ACTIVE` hold/release behavior. It does not by itself grant music or prove mixer load; `thAudio` must still separately request `SAI_AUDIO_ACTIVE` for actual speaker playback.
- Before deep sleep: drain or stop playback, stop DMA, stop BBB output, place `SD_MODE` low, release audio clock intent, and acknowledge quiesce.
- After STOP2, `thPower` must restore the active voltage scale and wait for both voltage-ready indications before it restarts PLL2P or grants SAI ownership. It then revalidates the PLL2 epoch, performs the bounded SAI mux handoff, and reports the resulting kernel clock through the requester ACK.
- After that power grant, `thAudio` must revalidate and re-arm the SAI data path and DMA before accepting playback. A stale pre-STOP HAL/DMA state is never sufficient evidence that the hardware is ready.

## Failure Policy

Audio failure is major but non-fatal.

On failure:

- quarantine audio path if recovery is not immediately possible
- keep shell/game running silently where possible
- publish visible/audible fault only if another output path remains valid
- preserve diagnostics for bring-up
- recover through bounded attempts only

Examples of audio faults:

- DMA underrun
- ADPCM decode failure
- mixer overrun or invalid voice state
- SAI/DMA error
- BBB pattern validation failure
- LPTIM/BUZZ output fault

## Validation Cases

1. speaker enable/idle/shutdown drives `SD_MODE` correctly
2. generated diagnostic tone proves bounded SAI DMA output but is not accepted
   as package-audio proof
3. one package-backed 16 kHz mono IMA ADPCM STATE SFX decodes, plays, drains,
   releases audio clock intent, permits STOP2, and plays again after a real
   STOP2 wake
4. a package-backed SFX longer than the two DMA halves refills without an
   underrun, then drains and releases the audio clock intent
5. invalid format, decoded-size, and missing-asset cases are rejected
   before playback
6. 1 music voice plus 5 overlapping SFX voices mix without underrun
7. SFX priority/preemption works deterministically
8. fade, mute, volume, and ducking behave correctly
9. music ring-buffer path never reads from FileX/FAT during active playback
10. BBB built-in pattern plays and stops cleanly on a target that grants BBB
11. BBB procedural requests validate bounds on a target that grants BBB
12. speaker and BBB play concurrently only on a target that grants both
13. quiesce/resume leaves no active DMA/LPTIM output stale
14. injected underrun routes to recovery or audio quarantine
15. installer mode isolates audio unless explicitly allowed for diagnostics

HW6 validation status: case 3 is target-proven with one Peep Studio-imported
STATE cue. The operator accepted the packaged cue's subjective fidelity and
heard it before and after STOP2. The final post-STOP capture showed successful
voltage-scale restoration, PLL2P re-arm, SAI mux handoff, `4.096 MHz` SAI grant,
DMA completion with IRQ/callback activity and no remaining transfer bytes,
clean speaker shutdown, released SAI clock/reset ownership, and a physically
ready STOP2 ledger with no failure mask. Case 4 remains pending target evidence.

Related:

- [[Audio_Index]]
- [[Subsystem_State_Machines]]
- [[Hardware_Index]]
- [[HW6_DMA_Map]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Power_Rails]]
- [[Power_and_Sleep_Policy]]
