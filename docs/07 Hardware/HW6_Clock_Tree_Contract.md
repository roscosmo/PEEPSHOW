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

## Clock Policy Model

HW6 clock control is an internal Platform policy, not a collection of subsystem-specific overrides. Owners publish state and capability needs; `thPower` selects and applies the clock profile at owner-safe boundaries.

The names below are internal policy labels. Any frequency shown is a candidate or imported IOC fact until HW6 measurement promotes it.

| Internal profile | Intended use | SYSCLK / HCLK class | Required kernel clocks | Status |
|---|---|---|---|---|
| `CLK_BOOT_RECOVERY` | boot, fault handling, debugger-safe recovery | current generated MSI/MSIS baseline: IOC records `24 MHz`; any `25 MHz` MSIS retune must be generated and measured before promotion | I2C3/MSIK for PMIC/input bring-up; LPUART/HSI only if communication owner is admitted | documented policy placeholder |
| `CLK_REACTIVE_BASE` | menus, input, PMIC monitor, static display hold, non-USB idle-active work | low MSI/MSIS class, expected `24/25 MHz` candidate range | display/I2C kernels only while their owners are active | unmeasured candidate |
| `CLK_REACTIVE_BURST` | bounded UI or shell transaction where racing back to STOP may save energy | mid PLL SYSCLK class, expected `40/48 MHz` candidate range | no extra kernel clocks unless an owner requests them | unmeasured candidate |
| `CLK_REALTIME_BALANCED` | runtime/gameplay with frame, input, sensor, display, and optional audio deadlines | measured realtime PLL class, expected `80 MHz` candidate before any higher point is granted | SAI/OCTOSPI only when their owning threads request those peripherals | unmeasured candidate |
| `CLK_IO_HIGH` | USB MSC/export, installer windows, and high-throughput storage windows | high PLL SYSCLK class; current ad hoc USB MSC `160 MHz` behavior belongs here | USB `48 MHz`; OCTOSPI kernel only if storage is active | scaffold target; not yet measured as policy |
| `CLK_STOP_PREP` | transition toward STOP2 or software shipment | no active high-speed requirement after owner quiesce | USB clock off, PLL2 off unless a validated autonomous scenario still owns it | scaffold pending |

Capability requests are also internal. They describe what must be true, not how to program RCC:

| Capability | Typical source | Policy effect |
|---|---|---|
| `USB_DEVICE_ACTIVE` | USB/installer/MSC export ownership | keep USB `48 MHz` valid, block STOP2, and usually select `CLK_IO_HIGH` while active |
| `OCTOSPI_ACTIVE` | `thStorage` package load/install/export transaction | keep OCTOSPI kernel valid; do not switch SYSCLK/PLL2 during an active bus transaction |
| `SAI_AUDIO_ACTIVE` | `thAudio` playback/mixer/DMA state | keep PLL2P/SAI valid; do not vary the audio sample/kernel clock because CPU policy changed |
| `DISPLAY_TRANSFER_ACTIVE` | `thDisplay` SPI/LPDMA transfer | keep display kernel stable until transfer completion; future LPBAM display ownership must be explicitly validated |
| `REALTIME_DEADLINE_ACTIVE` | runtime/gameplay admitted by Platform | choose the lowest measured realtime point with deadline margin |
| `REACTIVE_TRANSACTION_ACTIVE` | shell/menu/input transaction admitted by Platform | choose the lowest measured reactive point that completes the transaction and returns to the selected wait backend efficiently |

PLL2 is not a global always-on clock. PLL2P is justified by active SAI audio, and PLL2Q is justified by active OCTOSPI/storage work. If neither capability is active, the production policy should be able to turn PLL2 off after owner quiesce and clock readback validation.

USB clock is justified by an active USB-device capability only. Charger/VBUS presence by itself does not imply USB MSC/export ownership and must not automatically force the USB clock or high SYSCLK profile.

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

