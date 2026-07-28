# HW6 Delta From HW5

This document records the hardware, firmware, capability, and validation delta
between retired HW5 and active HW6.

Status: `pre_arrival`

## Confirmed Product Delta

| Area | HW5 | HW6 | Firmware / Contract Effect | Validation Effect |
|---|---|---|---|---|
| Ambient light | `TEMT6000X01`, `PC2 PHOT_EN`, `PC3 ADC1_IN4` | circuit removed; pins unassigned; `ADC1` absent from MCU/IP list | HW6 profile sets `sensor.light` unavailable; no light owner path or ADC sampling | no light bring-up; verify removed nets do not backfeed or draw current |
| Rotary encoder | `PA5 TIM2_CH1`, `PA1 TIM2_CH2`, `PB4 ENC_EN` | circuit removed; pins unassigned; `TIM2` absent from MCU/IP list and NVIC | HW6 profile sets `input.encoder` unavailable; remove encoder wake and fallback assumptions | no encoder bring-up; verify removed initialization and IRQ are absent |
| Piezo / BBB | `PAM8904EGPR`, `PB2 BUZZ`, physical `LPTIM1_CH1` output | driver and piezo path removed; `PB2` unassigned | HW6 profile sets `audio.bbb` unavailable; speaker audio remains | no BBB electrical/audio tests; validate speaker path only |

## Retained Internal Resource

`LPTIM1` remains in the HW6 IOC with internal/no-IO channel configuration for
LPBAM timing. Removing the physical piezo path does not make `LPTIM1`
available for unrelated reassignment.

## IOC Differences Requiring Resolution

| Difference | IOC Observation | Required Resolution | Status |
|---|---|---|---|
| Display translator enable | HW5 used software-controlled `PD2 VLT_LCD`; HW6 IOC has no `PD2` assignment | translator is hardwired enabled so EXTCOMIN always passes; no software OE state or duty cycling | `resolved_design` |
| Power debug marker | HW6 assigns `PH1 PWR_DBG` as GPIO output | spare output reaches the battery connector for PPK2 logic capture; Platform diagnostics owns idle-low, dev-only marker use | `resolved_design` |
| Stale generation list | `ProjectManager.functionlistsort` still names `MX_TIM2_Init` and `MX_ADC1_Init` | regenerate only after review; generated code must not initialize absent peripherals | `pending_generation_check` |

These are configuration deltas, not claims that additional physical parts were
added or removed.

## Retained Subsystems Requiring HW6 Regression

- ADP5360 rail, charger, fuel-gauge, MR/Start, and interrupt behavior
- LS013B7DH05 display, EXTCOMIN, translator path, SPI3, LPDMA, and LPBAM
- AT25SL128A OCTOSPI read/program/erase/deep-power-down behavior
- MAX98357A speaker output and SAI/GPDMA behavior
- A/B/L/R/Start/Boot input and wake paths
- TMAG3001 joystick and interrupt path
- LIS2DUX12 IMU, interrupt, and lowest-power mode
- NINA-B112 UART, hardware flow control, BLE, NFC, and low-power behavior
- USB device/MSC ownership and recovery
- STOP2, retained memory, reactive/realtime operating points, and wake/resume

Retained topology reduces expected implementation change. It does not waive
HW6 measurement.

## Compatibility Consequences

HW5 packages are not assumed compatible with HW6. Package validation must
reject an HW6 build when it requires any removed capability without an
admitted fallback.

No compatibility shim may synthesize encoder, ambient-light, or piezo hardware
events unless a future Engine contract explicitly defines such a portable
fallback.
