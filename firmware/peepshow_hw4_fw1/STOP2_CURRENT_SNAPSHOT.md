# STOP2 Current Snapshot

Implementation snapshot for the current STOP2 path at git `bbaa492e547aa6976a09d34355599fa8e895390c` on 2026-04-02.

This file is observational, not authoritative. The authority docs remain:

- `docs/authority.md`
- `docs/power_management.md`
- `docs/hardware.md`
- `docs/rtos_architecture.md`

## Scope

This is a code-derived baseline of what the firmware currently appears to do around STOP2.

It is meant to answer:

- what is explicitly quiesced before STOP2
- what clocks and oscillators are configured or forced for STOP usage
- what peripherals are actually shut down versus only left idle
- what RAM retention is explicitly configured
- what is still unknown without live register capture or current measurement

## Governing context

- Domain authority: `docs/authority.md`, `docs/power_management.md`, `docs/hardware.md`, `docs/rtos_architecture.md`
- Hard invariants touched:
  - single-owner peripheral model
  - STOP cadence is RTC-driven
  - quiesce-before-sleep
  - resume-before-validate
  - timebase correctness after clock changes
- Ownership impact:
  - `thPower` owns STOP2 entry/exit
  - STOP2 quiesce/resume currently coordinates `thDisplay`, `thStorage`, `thInput`, `thAudio`, and `thSensor`
- Mode impact:
  - primarily `STOP`
  - wake may briefly resume owners for STOP active windows, or fully exit to `STATIC`

## High-confidence baseline

### STOP2 entry/exit flow

- `thPower` arms STOP2 on `APP_MODE_STOP`, immediately starts quiesce, and waits for ack bits from display, storage, input, audio, and sensor owners.
  - Evidence: `Core/Src/threads/app_thread_entry_power.c:673`, `Core/Src/threads/app_thread_entry_power.c:859`
- Once quiesced, `thPower` arms the RTC wake timer, records telemetry, suspends the HAL tick, executes `HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI)`, resumes the HAL tick, then restores the normal clock profile.
  - Evidence: `Core/Src/threads/app_thread_entry_power.c:512`, `Core/Src/threads/app_thread_entry_power.c:904`
- Wake handling always re-applies the normal clock path before deciding whether to stay in STOP cadence or exit STOP mode.
  - Evidence: `Core/Src/threads/app_thread_entry_power.c:964`, `Core/Src/app_threadx.c:12173`

### Wake sources and wake handling

- STOP cadence is RTC wakeup timer driven.
  - Evidence: `Core/Src/main.c:648`, `Core/Src/threads/app_thread_entry_power.c:512`
- Button EXTI lines remain active while input is quiesced. A/B/L/R are the only button sources explicitly treated as STOP interaction wakes in software and replayed after resume.
  - Evidence: `Core/Src/main.c:860`, `Core/Src/threads/app_thread_entry_input.c:28`, `Core/Src/threads/app_thread_entry_input.c:42`, `Core/Src/threads/app_thread_entry_input.c:76`, `Core/Src/threads/app_thread_entry_input.c:109`
- Other EXTI sources are also configured at the GPIO level and can plausibly wake the MCU electrically, but the STOP interaction logic only special-cases A/B/L/R.
  - Evidence: `Core/Src/main.c:885`, `Core/Src/main.c:913`, `Core/Src/main.c:930`, `Core/Src/main.c:936`

### Debug measurement distortion

- If `KNOB_DEBUG_SWO_ENABLE != 0`, the firmware enables debug-in-STOP and debug-in-standby. That means debugger-attached current measurements may not represent real STOP2 behavior.
  - Evidence: `Core/Src/main.c:94`

## Clock and oscillator baseline

### Normal clock profile restored after wake

- `SystemClock_Config()` currently restores MSI as SYSCLK with:
  - MSI range `RCC_MSIRANGE_1` = 24 MHz on STM32U5
  - AHB `/1`
  - APB1 `/2`
  - APB2 `/2`
  - APB3 `/8`
  - HSI enabled
  - HSI48 enabled
  - LSE enabled
  - MSIK enabled
  - PLL1 disabled in the normal profile
  - Evidence: `Core/Src/main.c:186`, `Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_rcc.h:380`
- Important mismatch: `docs/hardware.md` still describes the baseline low-power SYSCLK as 16 MHz, but current code restores 24 MHz.

### Explicit STOP-related clock intent

- MSIK is explicitly forced to remain available in STOP mode with `__HAL_RCC_MSIKSTOP_ENABLE()`.
  - Evidence: `Core/Src/main.c:236`
- The current code does not explicitly force HSI, HSI48, PLL1, or PLL2 to remain active during STOP2.
- The code does explicitly restore PLL2-derived peripheral clocks after wake.
  - Evidence: `Core/Src/main.c:243`, `Core/Src/app_threadx.c:12144`

### Peripheral kernel clock choices

- SPI3 kernel clock source: MSIK.
  - Evidence: `Core/Src/stm32u5xx_hal_msp.c:582`
- I2C3 kernel clock source: MSIK.
  - Evidence: `Core/Src/stm32u5xx_hal_msp.c:106`
- LPUART1 kernel clock source: LSE.
  - Evidence: `Core/Src/stm32u5xx_hal_msp.c:185`
- SAI1 and OCTOSPI kernel clocks are sourced from PLL2.
  - Evidence: `Core/Src/main.c:243`, `Core/Src/app_threadx.c:12144`
- USB CLK48 uses HSI48.
  - Evidence: `Core/Src/stm32u5xx_hal_msp.c:730`

## Peripheral baseline

### Display: idle, not powered down

- Display quiesce clears pending present state, drops CS inactive, and keeps the LCD level translator enabled.
  - Evidence: `Core/Src/threads/app_thread_entry_display.c:27`, `Core/Src/app_threadx.c:2260`
- The display translator is hard-forced to its active level regardless of the `enabled` argument because EXTCOM must keep reaching the panel.
  - Evidence: `Core/Src/app_threadx.c:2260`
- I found no STOP quiesce path that de-inits SPI3, disables the SPI3 peripheral clock, or disables LPDMA1.
  - Evidence: `Core/Src/stm32u5xx_hal_msp.c:569`, `Core/Src/main.c:402`
- Current interpretation: the display datapath is left idle, but not aggressively power-gated.

### Storage / external flash: explicitly shut down

- Storage quiesce sends the AT25 flash into deep power-down, then disables OSPIM and OSPI1 clocks.
  - Evidence: `Core/Src/app_threadx.c:7902`, `Core/Src/app_threadx.c:7795`
- Storage resume re-enables OSPIM/OSPI1 clocks and releases deep power-down.
  - Evidence: `Core/Src/app_threadx.c:7866`, `Core/Src/app_threadx.c:7784`
- This is one of the cleanest current STOP2 power reductions in the codebase.

### Audio: explicitly shut down

- Audio quiesce calls `AppAudioStop()`.
  - Evidence: `Core/Src/threads/app_thread_entry_audio.c:85`, `Core/Src/app_threadx.c:3363`
- `AppAudioStop()` mutes the amp (`SD_MODE` low), stops SAI DMA, disables SAI, and disables the SAI1 peripheral clock.
  - Evidence: `Core/Src/app_threadx.c:3363`
- Audio start is demand-powered and only enables SAI1 when playback begins.
  - Evidence: `Core/Src/app_threadx.c:3306`

### Sensors: mostly software-suspended, not deeply powered down

- STOP and STATIC mode handling forces the LIS requested profile to `LOW_POWER`.
  - Evidence: `Core/Src/app_threadx.c:7096`
- Sensor quiesce clears polling state and marks PMIC, TMAG, and LIS as `SUSPENDED`.
  - Evidence: `Core/Src/threads/app_thread_entry_sensor.c:95`, `Core/Src/app_threadx.c:3929`
- I did not find STOP quiesce code that:
  - de-inits I2C3
  - disables the I2C3 peripheral clock
  - sends TMAG into a lower-power operating mode than its existing runtime configuration
  - puts the PMIC interface into a different electrical state
- The only I2C3 de-init I found is the bus-recovery path, not the STOP path.
  - Evidence: `Core/Src/app_threadx.c:3990`
- Current interpretation: sensor ownership is suspended at the thread level, LIS is nudged toward low-power settings, but the sensor rail and I2C block are not yet aggressively STOP-optimized.

### Input: logical quiesce only

- Input quiesce stops logical processing and suppresses queued raw events, but the EXTI/GPIO hardware remains active.
  - Evidence: `Core/Src/threads/app_thread_entry_input.c:109`, `Core/Src/main.c:860`
- That is intentional because STOP wake taps are captured while quiesced and replayed on resume.
  - Evidence: `Core/Src/threads/app_thread_entry_input.c:42`, `Core/Src/threads/app_thread_entry_input.c:76`

### USB: controller stopped, but boot-time clock enable is suspicious

- USB PCD is initialized at boot, which enables the USB peripheral clock and VDDUSB.
  - Evidence: `Core/Src/main.c:811`, `Core/Src/stm32u5xx_hal_msp.c:730`
- Outside active USB sessions, the power thread stops and disconnects the PCD and disables the OTG IRQ.
  - Evidence: `Core/Src/app_threadx.c:12920`, `Core/Src/app_threadx.c:12996`
- I did not find a normal STOP path that calls `HAL_PCD_DeInit()` or disables the USB peripheral clock after boot.
  - Evidence: `Core/Src/stm32u5xx_hal_msp.c:792`
- Current interpretation: USB traffic is stopped outside `FLASHING`, but the clock and VDDUSB setup may still be costing power.

### LoRa / LPUART1: initialized, but no STOP2 owner integration found

- LPUART1 is initialized at boot and its peripheral clock is enabled in MSP init.
  - Evidence: `Core/Src/main.c:430`, `Core/Src/stm32u5xx_hal_msp.c:171`
- I did not find current `thRadio` runtime usage or inclusion in the STOP2 quiesce ack mask.
  - Evidence: `Core/Src/app_threadx.c:473`, `Core/Src/threads/app_thread_entry_power.c:859`
- Current interpretation: this path needs explicit confirmation on hardware. It looks more like initialized scaffolding than an active STOP2-participating subsystem.

### DMA blocks

- GPDMA1 is enabled at boot and I found no explicit runtime clock disable.
  - Evidence: `Core/Src/main.c:288`
- LPDMA1 is enabled at boot and I found no explicit runtime clock disable.
  - Evidence: `Core/Src/main.c:402`
- Storage and audio stop their own transfers, but the DMA controller clocks themselves are not aggressively gated in code.

## RAM and retained state baseline

- `SystemPower_Config()` explicitly disables STOP retention only for:
  - DMA2D RAM
  - PKA32 RAM
  - Evidence: `Core/Src/main.c:275`
- I did not find code that disables STOP retention for the main SRAM banks.
- The retained runtime blob is explicitly placed in `.sram4`.
  - Evidence: `Core/Src/app_threadx.c:419`, `Core/Src/app_threadx.c:801`
- STOP2 telemetry is also written into RTC backup registers DR20-DR26.
  - Evidence: `Core/Src/threads/app_thread_entry_power.c:111`

Current interpretation:

- SRAM4 retention is intentionally relied upon.
- Other SRAM bank retention appears to be left at the platform default, but this should be confirmed with live PWR retention register capture before we claim it as fact.

## What seems to be on vs off in STOP2

### Most likely still on or intentionally retained

- RTC / backup domain / LSE, because STOP cadence depends on it
- MSIK stop availability, because it is explicitly forced in STOP mode
- SRAM4 retained state, because the firmware explicitly stores retained state there
- LCD translator path, because the code forces it active continuously
- button and sensor EXTI wake wiring

### Most likely off or intentionally shut down before STOP2

- CPU execution domain, by virtue of STOP2 entry
- HAL tick activity, because the code suspends it before STOP2
- audio path, because SAI1 clock is disabled and amp shutdown is asserted
- external flash active state, because flash deep power-down is entered and OSPI clocks are disabled

### Likely idle but not yet aggressively optimized

- SPI3 peripheral clock / display block
- I2C3 peripheral clock / sensor bus
- USB peripheral clock and VDDUSB setup
- LPUART1 / LoRa setup
- GPDMA1 and LPDMA1 controller clocks

## Unknowns that still need live capture

These items are not proven by source inspection alone:

- actual RCC oscillator and PLL state while the MCU is physically in STOP2
- whether USB peripheral clock and VDDUSB materially remain active through STOP2 in the current build
- which SRAM banks are truly retained versus just assumed retained
- whether any sensor silicon remains in a higher-than-necessary current mode during STOP2
- whether the display EXTCOM path depends on any unseen timer or external toggling load on the current board revision

## Immediate optimisation candidates

Based on the current baseline, the highest-suspicion STOP2 loads are:

1. USB clock and VDDUSB being set up at boot but not fully torn down outside `FLASHING`
2. SPI3 and LPDMA1 staying initialized even though display work is fully quiesced
3. I2C3 and attached sensors being software-suspended without deeper hardware sleep sequencing
4. LPUART1 / LoRa path being initialized without explicit low-power ownership or STOP2 teardown
5. Base normal clock profile restoring 24 MHz instead of the documented 16 MHz low-power baseline

## Live capture recipe

Source inspection is enough to rank suspects, but not enough to prove the real
STOP2 current baseline. The current should be measured with the debugger
detached, while the register snapshot should be captured immediately after a
STOP2 wake.

The repo now has two consolidated GDB helpers in `debug.gdb`:

- `ps_stop2_power_snapshot`
- `ps_stop2_power_snapshot_full`

Recommended capture flow:

1. Boot the current firmware and allow at least one STOP2 entry/wake cycle.
2. Halt right after wake, not while the debugger is holding the MCU inside STOP2.
3. Run `ps_stop2_power_snapshot` for the compact clock and power register dump.
4. Run `ps_stop2_power_snapshot_full` if you also want owner-thread and peripheral-handle state in the same capture.
5. Separately record STOP2 current with the debugger fully detached.

The minimum hardware baseline record should include:

- RCC `CR`, `CFGR1`, `BDCR`, `CCIPR1/2/3`, ENR and SMENR registers
- PWR `CR1/2/3`, `VOSR`, `SVMCR`, `SVMSR`, `SR`, `WUSR`, `BDCR1/2`, `UCPDR`
- RTC wake timer registers
- HAL handle states for USB, OCTOSPI, SPI3, I2C3, SAI1, and LPUART1
- detached STOP2 current for the same firmware image and board setup

Important measurement caveat:

- attached-debug current is not trustworthy for STOP2 optimisation decisions
- the post-wake register snapshot is trustworthy for "what resumed enabled"
- the detached ammeter reading is trustworthy for "what the board actually draws"

## Recommended next step

Before changing code, capture one live STOP2 register/current snapshot with the
new GDB helper plus a detached current measurement. That will turn this
code-derived baseline into a proven hardware baseline and avoid optimizing the
wrong subsystem first.
