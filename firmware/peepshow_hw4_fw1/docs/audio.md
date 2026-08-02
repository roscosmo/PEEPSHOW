# Audio Subsystem

Authoritative specification for audio playback, SAI configuration,
DMA buffering, runtime asset sourcing, and power coordination
in PeepShow V5.

This document defines how audio is produced, where audio data comes from,
how buffers are managed, and how the subsystem interacts with power control.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- SAI1 configuration and ownership
- Audio DMA buffer discipline
- Runtime asset source model (installed blobs)
- Playback contract (SFX + music)
- Power coordination (PLL2P usage)
- Determinism and latency requirements

Does NOT define:
- Blob file format (see asset_pipeline.md)
- Storage installation model (see storage_and_updates.md)
- Clock governor policy (see power_management.md)

---

## Design Principles

- Audio must be deterministic once playback begins.
- Audio must not depend on FileX at runtime.
- All audio data must originate from installed blob storage or system resources.
- Only thAudio owns SAI1 and audio DMA.
- Audio must stop cleanly before STOP2.
- No blocking waits inside thAudio.

---

## Hardware Configuration

Interface:
- SAI1 (I2S transmit)
- Mono
- 16-bit samples
- 16 kHz sample rate

Master clock:
- PLL2P at 4.096 MHz (256 × FS)

Amplifier:
- MAX98357A
- SD_MODE:
  - LOW  = shutdown
  - HIGH = active
- Must default LOW at boot

Only thAudio may:
- Enable/disable PLL2P (via thPower request)
- Drive SD_MODE
- Start/stop SAI DMA

---

## Ownership Model

Owned exclusively by thAudio:

- SAI1 peripheral
- Audio DMA channel
- DMA buffers
- Amplifier control pin
- Playback state machine

Other threads must not:
- Touch SAI HAL
- Manipulate DMA buffers
- Toggle SD_MODE
- Reconfigure PLL2P directly

Clock requests must go through qSysEvents to thPower.

---

## Runtime Asset Source

Audio assets are loaded from:

- Raw Installed Blobs region
- System/public resources (small always-present assets)

Audio must not:
- Stream from FileX
- Read directly from FAT during playback

When a game blob is activated:
- Audio chunks are located via blob manifest
- Required audio data is:
  - fully loaded into RAM
  OR
  - staged in a bounded ring buffer

Once playback begins:
- Audio reads only from RAM or deterministic raw flash offsets
- No filesystem metadata lookups occur

---

## Playback Model

Two playback types:

1. Short SFX
2. Background music (looping or long-form)

### Short SFX

- Fully loaded into RAM before playback.
- Can overlap via simple mixer.
- Must not block.

### Background Music

Two allowed models:

A) Fully preloaded into RAM (preferred for simplicity)
B) Chunked read from raw installed blob region into ring buffer

Chunked read rules:
- Reads must be bounded and deterministic.
- No FAT involvement.
- No clock switching during active transfer.

---

## DMA Buffering Discipline

Audio uses double-buffer or circular DMA mode.

DMA interrupts:
- Half-complete
- Complete

ISR rules:
- ISR sets thread flag only.
- No mixing inside ISR.
- No heavy math inside ISR.

thAudio on flag:
- Refills half-buffer from mixer or asset data.
- Must complete refill before next half-cycle.

Underruns are unacceptable in steady-state playback.

---

## Mixer Model

Mixer constraints:

- Fixed number of channels (compile-time limit).
- 16-bit intermediate accumulation.
- Clamped to int16 range.
- No floating point required.

Mixer must:
- Be bounded in execution time.
- Fit within half-buffer refill window.
- Not allocate memory.

---

## Latency Requirements

Audio start latency:
- Minimal delay between play request and audible output.

Policy:
1. thAudio receives play command.
2. Requests PLL2P enable via thPower.
3. Waits for clock stable signal.
4. Enables SAI.
5. Raises SD_MODE.
6. Starts DMA.

Amplifier enable must occur only after valid data is ready.

---

## Power Coordination

PLL2P usage:

- Enabled only while audio active.
- Disabled when no playback active.
- Reference counting handled in thPower.

Before STOP2:
- DMA stopped.
- SAI disabled.
- SD_MODE driven LOW.
- PLL2P reference released.

Audio must acknowledge quiesce quickly.

---

## Determinism Requirements

- Playback must not depend on FAT.
- No blocking reads during playback.
- No dynamic memory allocation.
- No clock reconfiguration mid-DMA.
- Mixer execution time must be bounded.

---

## Failure Handling

If audio asset invalid:
- Playback request must fail gracefully.
- System must remain stable.

If buffer underrun detected:
- Stop playback cleanly.
- Report error via SWO (rate-limited).

Audio must never crash the system.

---

## Forbidden Patterns

- Streaming from FileX during playback.
- Blocking in ISR.
- Using malloc/free.
- Modifying clocks outside thPower.
- Accessing raw flash outside thStorage.

---

## Integration Notes

- Game engine issues playback requests via qAudioCmd.
- Storage layer supplies asset metadata only at load time.
- Display and audio must not contend for DMA channels.
- Audio must tolerate rapid start/stop transitions during mode changes.

---

Last updated: 2026-02-18
