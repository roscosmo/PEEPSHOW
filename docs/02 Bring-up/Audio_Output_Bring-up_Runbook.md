# Audio Output Bring-up Runbook

This runbook records the measured HW5 procedure for speaker and BBB audio paths.

> [!warning] HW6 applicability
> HW6 retains only the SAI/MAX98357A speaker path. The PAM/piezo BBB path and physical `PB2` output are absent, so every BBB procedure and result below is HW5 historical evidence only. HW6 `LPTIM1` belongs to the LPBAM timing path and must not be configured or claimed by audio.

Related:

- [[Audio_Contract]]
- [[HW5_DMA_Map]]
- [[HW5_Power_Rails]]
- [[Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- `MAX98357AETE+T` speaker path on `SAI1_A`
- `PC9` `SD_MODE` shutdown/enable behavior
- 16 kHz mono PCM playback
- DMA circular buffer behavior
- mixer preload path for music/SFX
- `PAM8904EGPR` piezo BBB path on `PB2` / `LPTIM1_CH1`
- simultaneous speaker and BBB policy if tested
- audio fault and recovery behavior

---

## CubeMX / Clock Baseline

| Path | MCU resources | Required baseline |
| --- | --- | --- |
| Speaker serial audio | `SAI1_A`: `PA8` SCK, `PB9` FS, `PA10` SD | master transmit, mono, 16-bit PCM, 16 kHz |
| Speaker DMA | `GPDMA1_CH3`, `GPDMA1_REQUEST_SAI1_A` | memory-to-peripheral, circular, halfword PCM samples |
| Speaker amp shutdown | `PC9` `SD_MODE` | low = MAX98357A shutdown, high = active |
| BBB piezo | `PB2` `BUZZ` / `LPTIM1_CH1` | procedural tone/sweep/pattern output |

SAI1 audio must use `PLL2P = 4.096 MHz`, which is `256 x 16 kHz`. Bring-up should prove this clock path before validating playback quality.

The speaker and BBB paths share one Platform owner, `thAudio`, but they are different output classes. Speaker output is sampled/mixed PCM. BBB output is symbolic/procedural tone generation and must not become a second PCM engine.

---

## Baseline State Sequence

This sequence validates the hardware path before full asset playback exists.

1. Boot with `SD_MODE` low and no `BUZZ` output.
2. Confirm MAX98357A shutdown current/behavior with `SD_MODE` low.
3. Configure SAI1 from the `PLL2P = 4.096 MHz` audio clock.
4. Enable `SD_MODE` high and allow the speaker amp to settle.
5. Play a generated 16 kHz mono PCM test tone through the speaker path.
6. Confirm DMA half/full service evidence without underrun.
7. Stop playback, drain DMA, and return `SD_MODE` low.
8. Generate a single BBB tone through `LPTIM1_CH1` / `BUZZ`.
9. Generate a BBB sweep and a short melody/pattern sequence.
10. Stop BBB output and confirm the PAM8904 path becomes idle without firmware holding a stale waveform.
11. Run speaker and BBB concurrently, if electrical/audio behavior is acceptable.
12. Quiesce audio before sleep and confirm no active SAI, DMA, LPTIM, or amp-enable state remains.

---

## Mixer / Asset Bring-up Sequence

These tests occur after the storage/package asset path is available.

1. Decode one 4-bit IMA ADPCM SFX asset into bounded 16-bit PCM buffers.
2. Play one decoded SFX without FileX/FAT reads during active playback.
3. Play music as the single music voice from a preloaded buffer or raw installed blob ring buffer.
4. Overlay up to five SFX voices over music and measure underrun behavior.
5. Validate SFX priority and preemption.
6. Validate fade, mute, volume, and ducking.
7. Inject invalid ADPCM or mixer state and prove bounded recovery or audio quarantine.

No active playback path may depend on heap allocation or host-exported FAT/FileX reads.

---

## BBB Procedure Matrix

| BBB request | Purpose | Acceptance evidence |
| --- | --- | --- |
| fixed tone | prove basic PAM/LPTIM path | frequency and duration close to request |
| gap/tone pattern | prove step scheduler | gaps do not leave unintended output |
| sweep | prove procedural frequency update | monotonic or expected curve |
| short melody | prove multi-step bounded sequence | sequence ends cleanly |
| invalid frequency | prove validation | request rejected without output fault |
| too-long sequence | prove bounds | request rejected or clipped by policy |
| high-priority BBB over low-priority BBB | prove preemption | deterministic stop/preempt behavior |

---

## Command / Configuration Ledger

Populate this table during bring-up. The current values are placeholders until measured on HW5 hardware.

| Step | Configuration | Expected result | Measured result | Status |
| --- | --- | --- | --- | --- |
| speaker idle | `SD_MODE` low | MAX98357A shutdown | `fw0` Phase 3A BBB probe forced `SD_MODE` low before and after the BBB tone; GDB readback showed `sd_mode_state_before=0` and `sd_mode_state_after=0`. Speaker amp shutdown current/behavior has not yet been measured separately. | partial |
| audio clock | `PLL2P = 4.096 MHz` | 16 kHz mono frame timing | `fw0` SAI1 probe used `PLL2P = 4.096 MHz`; firmware readback showed `sai_kernel_hz=4096000` and `speaker_sample_rate_hz=16000` | pass |
| speaker tone | generated PCM | audible clean tone, no DMA underrun | `fw0` generated a `1 kHz` SAI1 TX DMA PCM tone with `SD_MODE` high; first run was silent due to a loose speaker connection, then user replugged/rebooted and heard a loud tone. A follow-up PPK2 ladder showed amp-on silence / sine amplitude `512` / sine amplitude `1500` were all about `10 mA` and too soft, sine amplitude `3000` was about `15 mA` and subjectively good, and a short square amplitude `1000` was about `10 mA` and also acceptable. | pass |
| DMA service | circular buffer | half/full callbacks or equivalent service events | SAI TX DMA probe showed `speaker_start_status=0`, `speaker_stop_status=0`, `speaker_half_cplt_count=50`, `speaker_cplt_count=49`, `speaker_error_count=0`, `sai_error_after=0` | pass |
| speaker stop | stop/drain path | DMA stopped, `SD_MODE` low | SAI TX DMA probe stopped with `speaker_stop_status=0`, `sai_state_after=READY`, `sai_error_after=0`, and `sd_mode_state_after=0` | pass |
| BBB tone | LPTIM1 fixed frequency | piezo tone generated | `PB2` / `LPTIM1_CH1` generated a one-shot `2 kHz` active-high PWM tone for about `500 ms` from MSIK `4 MHz`; GDB readback showed `period=1999`, `pulse=1000`, all HAL statuses `0`, and user heard a beep | pass |
| BBB sweep | bounded sweep | sweep generated and stopped cleanly | `PB2` / `LPTIM1_CH1` generated a rising sweep from `800 Hz` to `4000 Hz` over `24` steps at `35 ms` per step; GDB readback showed all `24` steps completed, no failed step, start/stop/final step statuses `0`, and user heard the sweep | pass |
| BBB melody | bounded sequence | pattern completes and PAM idles | Six-step BBB melody/gap pattern completed in firmware with `6` requested and `6` completed steps, no failed step, and clean start/stop statuses; user heard the melody but perceived it closer to five tones, likely due to timing or piezo response masking one step | pass_with_note |
| concurrent output | speaker plus BBB | both paths active without ownership conflict | TBD | open |
| ADPCM SFX | decoded asset | valid SFX playback without FAT runtime reads | TBD | open |
| mixer budget | music plus 5 SFX | no underrun at target load | TBD | open |
| fault injection | underrun/invalid asset | bounded recovery or audio quarantine | TBD | open |

---

## Validation Procedure

1. Confirm `SD_MODE` low places MAX98357A in shutdown.
2. Confirm SAI1 uses `PLL2P = 4.096 MHz` and produces 16 kHz mono frame timing.
3. Play a simple PCM test tone through speaker.
4. Validate DMA half/full callbacks or equivalent buffer service evidence.
5. Validate at least one ADPCM-decoded SFX path when available.
6. Validate music plus SFX mixing budget when mixer exists.
7. Generate BBB single tone through `BUZZ`.
8. Generate BBB pattern/melody sequence.
9. Validate PAM auto-shutdown behavior when `BUZZ` becomes idle.
10. Validate audio stop/quiesce before low-power transition.
11. Validate runtime rejects out-of-bounds BBB tone/sweep/sequence requests.
12. Validate audio failure is major but non-fatal and does not crash the shell/game runtime.

---

## Evidence Requirements

Record in [[Brought_Up_Tracker]]:

- SAI `PLL2P = 4.096 MHz` and 16 kHz frame observations
- speaker tone result
- BBB tone result
- shutdown behavior result
- DMA/mixer result if implemented
- measured current delta if available
- any underrun/fault observations

Audio failure is major but non-fatal according to [[Audio_Contract]].
