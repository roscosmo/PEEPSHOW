# EV-HW6-20260731-P5-OWNERS-002

## Summary

- Test case: queued HW6 `thPower`, `thDisplay`, and `thAudio` owner workflow
  with bounded event acknowledgements and physical display/speaker output
- Result: `PASS/PARTIAL`
- Date/time: `2026-07-31 AEST`
- Hardware target: `HW6`
- Board ID/serial: `HW6-UNIT-001` (provisional)
- Hardware: display and speaker attached; housing, buttons, and joystick controls
  not attached
- Hardware rework state: no rework recorded for this test
- Firmware base commit: `9b3f664635b9a31c0e36e2e44137eb800d9bbe1a`
- Firmware state: uncommitted/untracked HW6 FW0 owner-service diagnostic derived
  from the base commit
- Build profile: `Debug`
- FW0 ELF SHA-256:
  `1E67FC6021D52F30C63B699016AD8352D2065A7B0DA98262C9E7978ACB18FD1D`
- FW0 IOC SHA-256:
  `C3EA72AD444196CA2C3053D3F4F11B5462B3D4978CF9295744C341A493D90DA8`
- Instrumentation: ST-LINK SWD/GDB and direct operator observation; no current
  claim is made by this artifact

## Setup

The existing bounded pre-kernel device probe ran first. ThreadX then delivered
a fixed workflow command to `thPower` through `qSysEvents`. `thPower` asserted
`PWR_DBG`, read seven ADP5360 registers through the serialized I2C3 lease, sent
a display command through `qDisplayCmd`, and waited for a bounded `egDebug`
acknowledgement. It then sent an audio command through `qAudioCmd`, waited for
the second acknowledgement, and returned `PWR_DBG` low.

`thDisplay` cleared the panel and transferred a deterministic 144 x 168
diagnostic frame through SPI3 and LPDMA1. `thAudio` enabled `SD_MODE`, played a
1 kHz, 16 kHz-sampled circular-DMA tone for about 750 ms, stopped SAI/DMA, and
returned `SD_MODE` low.

The operator allowed the workflow to finish, confirmed the physical outputs,
halted without reset, and sourced the single consolidated GDB report.

## Artifacts

| Artifact | Type | Purpose |
|---|---|---|
| `owner_workflow_gdb.log` | `hw_log` | Exact boot, retained-device, RTOS, queued-owner, PMIC, display, audio, and final-state snapshot |

## Results

- The retained-device baseline remained passing with required, attempted, and
  pass masks `0x1F`; failure mask was zero.
- The RTOS owner, queue, and event-group masks remained complete at
  `0x1FF`, `0x1FF`, and `0x0F`, with zero queue message errors.
- Owner-workflow probe identity/version/phase was
  `0x48364F57 / 1 / 0x67FF`; complete and success were both `1`.
- Workflow start/end were ticks `1 / 80`, a 79-tick or 790 ms transaction at
  the configured 100 Hz ThreadX tick rate.
- `thPower` read ADP5360 registers `00 29 2A 2B 2C 2E 2F` as
  `10 31 18 18 13 00 07`. Every mutex acquire/release and HAL transfer status
  was zero; identity, rails-ready, and fault-clear checks passed.
- `thDisplay` received its queued command at tick 2. The generated framebuffer
  hash and black-pixel count matched `0x360CDA71 / 1725`. RTC calibration output
  remained enabled, SPI/LPDMA init and transfer statuses were zero, DMA
  completed, and the operator saw the expected axes and corner squares.
- `thAudio` received its queued command at tick 3. SAI kernel, sample, and tone
  rates were `4096000 / 16000 / 1000 Hz`; start and stop statuses were zero,
  no SAI or DMA error remained, and the operator heard the tone.
- `SD_MODE` transitioned `0 -> 1 -> 0` around audio playback.
- Display and audio acknowledgement flags were `0x2` and `0x4`, with all send,
  wait, and set statuses zero.
- `PWR_DBG` recorded exactly two transitions and ended low at tick 80.

## Conclusion

HW6 unit 001 passes the first real RTOS owner-service slice. The result proves
bounded producer/consumer routing into the display and audio owners, serialized
PMIC access by the power owner, physical full-frame display output, audible SAI
DMA output, acknowledgement return paths, and clean amplifier/debug outputs at
workflow completion.

## Scope And Follow-Ups

This artifact does not close Phase 5. It does not validate queue saturation,
owner fault propagation, quiesce/resume, STOP2, LPBAM, partial display updates,
speaker current, mixer refill/underrun behavior, buttons, joystick behavior, or
repeated lifecycle operation. The automatic boot workflow is diagnostic and
must become an explicit test path as production owner services replace it.
