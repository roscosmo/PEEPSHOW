# HW6 Clock Tree Contract

This note records the imported HW6 IOC baseline. It does not freeze shipping
reactive or realtime operating points.

## IOC Baseline

| Clock | IOC Value | Use / Constraint |
|---|---|---|
| SYSCLK / HCLK / Cortex | `24 MHz` | generated full-intent baseline |
| APB1 | `12 MHz`, timers `24 MHz` | peripheral bus |
| APB2 | `12 MHz`, timers `24 MHz` | peripheral bus |
| APB3 | `3 MHz` | low-speed peripheral bus |
| MSI / MSIK | `24 MHz` / `4 MHz` | system and selected kernel sources |
| HSI / HSI48 | `16 MHz` / `48 MHz` | UART and USB-related kernels |
| RTC source | external nominal `32.768 kHz` on `PC14` | timekeeping and wake |
| PLL2P | `4.096 MHz` | SAI1 16 kHz audio basis |
| PLL2Q | `128 MHz` | OCTOSPI1 kernel baseline |
| SPI3 | MSIK `4 MHz`, prescaler 2, calculated `2 Mbit/s` | display transfer and LPBAM |
| I2C3 | MSIK `4 MHz`, timing `0x00000E14` | PMIC, joystick, IMU |
| LPUART1 | HSI `16 MHz`, `115200` baud | NINA transport |
| USB | `48 MHz` | USB device |

The IOC reports SAI1 real audio frequency `16.0 kHz` and also serializes an
`ErrorAudioFreq` warning of `-91.66%`. The intended sample rate and generated
register result must be reconciled before HW6 audio validation.

OCTOSPI1 uses kernel `128 MHz` with clock prescaler `8`. Effective bus timing
and signal integrity remain measured bring-up facts.

`LPTIM1` is retained as an internal/no-IO autonomous-display timer. Its actual
kernel selection and cadence register derivation must be verified in generated
code and HW6 LPBAM evidence.

## Operating-Point Families

| Family | Optimization Objective | Status |
|---|---|---|
| Boot / recovery | deterministic startup and debugger recovery | pending HW6 validation |
| Reactive active burst | minimize complete event-to-yield energy subject to response limits | pending HW6 sweep |
| Realtime sustained | lowest sustained power satisfying worst-case frame/audio/sensor/display deadlines | pending HW6 sweep |
| USB installer | satisfy USB timing with valid 48 MHz kernel | pending HW6 validation |

`thPower` owns operating-point selection. Packages express runtime semantics,
not SYSCLK, voltage scale, PLL, flash latency, or kernel-clock choices.

## Transition Rules

- no clock/voltage transition during active DMA or bus transactions
- peripheral kernels retain their functional constraints independently of CPU
  optimization
- STOP resume restores the selected point before owners resume work
- switching latency, energy, and hysteresis require HW6 evidence
- HW5 operating-point results are experimental baselines, not HW6 grants

## Removed Clock Consumers

- no ADC1 ambient-light sampling
- no TIM2 encoder interface or encoder IRQ
- no physical LPTIM1/PB2 piezo output

Related:

- [[Power_and_Sleep_Policy]]
- [[Power_Validation]]
- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[Pending_Measured_Constants_Register]]

