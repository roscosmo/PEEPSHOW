# EV-HW6-20260801-P5-AUDIO-009

## Summary

- Test case: lifecycle-v8 `ps_dev_audio` driver-backed owner SAI/GPDMA speaker tone cycle
- Result: `PASS`
- Date/time: `2026-08-01 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; USB/VBUS detached for the storage owner baseline
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `78a612b1a2fee96d3361a33eb8de7925adec2b08`
- Firmware state: uncommitted/untracked HW6 FW0 `ps_dev_audio` wrapper integration
- Build profile: `Debug`
- FW0 ELF SHA-256: `52A2B3A8FBF1D0CD3DFEDDF5AA01D0E73AB880CF0ADD23201DBD7039AE67271A`
- Instrumentation: ST-LINK SWD/GDB; `PWR_DBG` bounded the requested workflow

## Setup

The operator armed the lifecycle workflow with `__fw0_owner_sm_start.gdb`,
continued the target, halted without reset after completion, and sourced the
read-only consolidated report `__fw0_all_probe_prints.gdb`.

The run used `ps_dev_audio` from the audio owner path. The wrapper owns the
MAX98357A `SD_MODE` GPIO, SAI1/GPDMA transmit start, bounded tone duration,
DMA stop, final amp-off verification, and SAI/DMA error telemetry for this
speaker-only HW6 path.

## Artifacts

| Artifact | SHA-256 | Purpose |
|---|---|---|
| `owner_lifecycle_audio_driver_gdb.log` | `108760C6C1A93D7E59FCE0625D15BC9B3F9CB24B0FD26BF9BAFE1EDBA4CFB60F` | Exact target-memory and transition-trace report from the operator transcript |

The original operator transcript hash was
`108760C6C1A93D7E59FCE0625D15BC9B3F9CB24B0FD26BF9BAFE1EDBA4CFB60F`.

## Observations

- The operator heard the expected three approximately 750 ms 1 kHz tones.
- Top-level owner workflow completed with `complete/success/init = 1 / 1 / 0x0`.
- Audio owner completed with `command/complete/success = 778 / 1 / 1`.
- The audio driver probe reported
  `audio driver API/init/state/ops/last = 1 / 0x0 / 3 / 3 / 0x0`.
- SAI/sample/tone telemetry matched `4096000 / 16000 / 1000`.
- Duration/amplitude/buffer telemetry matched `750 ms / 3000 / 2048 halfwords`.
- `SD_MODE` moved through the expected disabled/enabled/disabled sequence `0 / 1 / 0`.
- SAI DMA start/stop returned `0x0 / 0x0`.
- SAI state/error after the run were `0x1 / 0x0`; DMA state/error after the run were `0x1 / 0x0`.
- Top-level lifecycle completed with `complete/success = 1 / 1`.
- Required/completed masks were `0x7F / 0x7F`; success/failure owner masks were `0x7F / 0x00`.
- Both bounded cycles completed with resume and quiesce success/failure masks `0x7F / 0x00`.
- Both cycles matched active and inactive ten-FSM state masks `0x3FF / 0x3FF`.
- `PWR_DBG` returned low at the end: `state/toggles/last = 0 / 2 / 1704`.

## Conclusion

The audio-owner lifecycle path now uses a typed `ps_dev_audio` wrapper instead
of direct owner-local SAI/GPDMA and `SD_MODE` operations. The wrapper-backed
path preserved the validated 4.096 MHz SAI kernel, 16 kHz PCM DMA tone, audible
speaker output, bounded stop, and amp shutdown behavior across the baseline
workflow plus two bounded owner-routed resume cycles.

This evidence does not close refill/underrun behavior, mixer/SFX playback,
ADPCM asset handling, volume/fade/mute policy, audio current, STOP2 audio
quiesce current, or audio fault injection/recovery.