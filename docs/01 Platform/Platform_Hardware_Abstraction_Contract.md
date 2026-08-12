# Platform Hardware Abstraction Contract

This document defines the Platform-internal hardware abstraction boundary for the active HW6 target.

The Platform hardware abstraction is not the Engine API and is not a game-authoring API. It is the controlled boundary between Platform owner threads and CubeMX-generated STM32 HAL/LL code.

Related:

- [[Authority_and_Invariants]]
- [[Architecture_and_Boundaries]]
- [[RTOS_Ownership_and_Queue_Topology]]
- [[Peripheral_Robustness_Contract]]
- [[HW6_Hardware_Revision_Contract]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_DMA_Map]]
- [[HW6_Clock_Tree_Contract]]
- [[HW6_Power_Rails]]
- [[HW6_Wake_Sources]]
- [[HW6_Brought_Up_Tracker]]

---

## Scope

Defines:

- the boundary below Platform owner threads
- allowed access to CubeMX, STM32 HAL, LL, middleware, and generated handles
- board-support responsibilities
- device-driver responsibilities
- shared-bus serialization rules
- GPIO safe-default rules
- DMA, IRQ, timeout, sleep, and resume rules
- bring-up evidence expectations for hardware-facing code

Does not define:

- Engine-facing game-development APIs
- Reference Game mechanics
- package authoring contracts
- scene, input-focus, or asset APIs
- Platform owner-thread state machines
- CubeMX pin, clock, or DMA authority; see [[Hardware_Index]]

---

## Layer Position

```text
Engine / Reference Game / game-authoring tools
        |
Platform capability APIs and queues
        |
Platform owner threads
        |
Platform hardware abstraction and device drivers
        |
CubeMX / STM32 HAL / LL / middleware
        |
HW6 board
```

Rules:

- Engine, Reference Game, packages, and game-authoring tools must never call the Platform hardware abstraction directly.
- Platform owner threads consume the hardware abstraction through approved `ps_hw_*` and device-driver APIs.
- CubeMX-generated handles and direct STM32 HAL/LL access remain below this boundary.

---

## Terminology

`STM32 HAL` means the vendor HAL generated or configured by CubeMX.

`Platform hardware abstraction` means the PeepShow Platform-internal wrapper layer above STM32 HAL/LL and below owner threads.

`Device driver` means a Platform-internal module for a specific target device, such as the retained HW6 `ADP5360`, `LIS2DUX12TR`, `TMAG3001A1YBGR`, `AT25SL128A`, `LS013B7DH05`, or `NINA-B112-04B`.

The phrase `HAL abstraction` should be avoided in file and API names because STM32 already uses `HAL`.

---

## Authority

The Platform hardware abstraction is owned by the Platform.

Only Platform owner threads and Platform boot/fault infrastructure may use it.

Allowed consumers:

- `thPower`
- `thDisplay`
- `thAudio`
- `thInput`
- `thSensor`
- `thStorage`
- `thComm`
- boot and fault supervisor code during early boot or fault capture
- controlled bring-up and diagnostic code

Forbidden consumers:

- Engine runtime hosts
- package code
- Reference Game code
- game-authoring tools
- asset pipeline output

---

## Ownership Rules

- Every peripheral and shared datapath still has exactly one Platform owner as defined by [[HW6_Pin_Ownership_Matrix]] and [[RTOS_Ownership_and_Queue_Topology]].
- The hardware abstraction performs bounded hardware actions only.
- Owner threads own policy, sequencing, state machines, retries, capability publication, and user-visible behavior.
- A hardware abstraction API must not silently start a Platform mode transition, change runtime class, mount storage, launch recovery UI, or publish Engine/Game events.
- Cross-thread calls into hardware abstraction APIs are allowed only when the target hardware path is owned by the calling owner or explicitly permitted by a shared-bus contract.

Example:

```text
thDisplay decides when a display flush is allowed.
ps_hw_display_start_flush() performs the bounded SPI/LPDMA operation.
```

---

## Forbidden Access

The following are forbidden outside approved Platform hardware files:

- direct use of CubeMX-generated peripheral handles such as `hi2c3`, `hspi3`, `hsai_BlockA1`, `hlpuart1`, `hospi1`, `hadc1`, or DMA handles
- direct STM32 HAL/LL register or peripheral calls from Engine, Reference Game, package, or tool-facing code
- direct GPIO writes from non-owner code
- direct DMA start/abort from non-owner code
- direct EXTI, IRQ, or callback policy outside the owning abstraction path
- direct FileX, LevelX, USBX, or flash access outside `thStorage` and approved storage internals

Bring-up experiments may use temporary direct access only when recorded in [[Brought_Up_Tracker]] or a runbook temporary-measure entry, with an explicit exit criterion.

---

## API Naming And Placement

Platform hardware abstraction APIs should use the `ps_hw_` prefix.

Device-driver APIs should use a Platform-internal device prefix under the owner path, for example:

- `ps_hw_i2c3_*`
- `ps_hw_gpio_*`
- `ps_hw_dma_*`
- `ps_hw_clock_*`
- `ps_dev_adp5360_*`
- `ps_dev_lis2dux12_*`
- `ps_dev_tmag3001_*`
- `ps_dev_at25sl128a_*`
- `ps_dev_ls013b7dh05_*`
- `ps_dev_nina_b112_*`

Recommended future source layout:

```text
firmware/platform/src/hw/board/
firmware/platform/src/hw/bus/
firmware/platform/src/hw/dma/
firmware/platform/src/hw/devices/
firmware/platform/src/hw/irq/
firmware/platform/src/hw/timebase/
firmware/platform/include/platform/hw/
```

Public Engine headers must not include files from `platform/hw`.

---

## Board Support Responsibilities

The board-support layer owns low-level HW6 board facts that owner threads need but should not duplicate.

Responsibilities:

- apply safe GPIO defaults during boot
- centralize active polarity helpers
- expose board revision and hardware configuration constants
- validate required clocks and timebases
- provide clock-profile apply/restore helpers to `thPower`
- provide wake-source capture and low-level reset-reason capture
- provide IRQ demux hooks that signal owner threads
- wrap DMA start, completion, abort, and error paths
- hide CubeMX-generated handle names

Board-support code must agree with:

- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_CubeMX_Pin_Map]]
- [[HW6_Clock_Tree_Contract]]
- [[HW6_DMA_Map]]
- [[HW6_Power_Rails]]
- [[HW6_Wake_Sources]]

## Clock-Profile Abstraction

Board support provides the low-level clock-profile helper API used by `thPower`. No device driver, package, runtime module, UI code, or non-power owner may call generated clock configuration, RCC, FLASH latency, voltage-scaling, USB clock, PLL, SAI clock, or OCTOSPI clock controls directly.

Clock helper responsibilities:

- validate that a requested internal profile is supported by the compiled HW6 target
- apply voltage scale, flash latency/cache state, SYSCLK/HCLK source, bus prescalers, and affected peripheral kernel clocks in the documented safe order
- enable or disable optional clock roots such as USB clock and PLL2 only when their capabilities are active
- read back enough RCC/FLASH/PWR state for probe and TraceX evidence
- return bounded status without hidden retries or unbounded waits
- provide restore helpers after STOP/resume before owner work resumes

The helper layer does not choose policy. It applies the profile selected by `thPower` and reports whether the hardware actually reached that state.

---

## Device Driver Responsibilities

Device drivers own device protocol details, not Platform policy.

Allowed device-driver responsibilities:

- identity and liveness probe
- register read/write helpers
- command framing
- required delays and readbacks
- device-local conversion helpers where they are hardware facts
- bounded transaction execution
- typed hardware error reporting

Forbidden device-driver responsibilities:

- game input mapping
- Engine capability decisions
- sleep-class decisions
- storage region policy
- package install decisions
- unbounded retries
- user-interface decisions
- hidden dynamic allocation

Examples:

```text
ps_dev_tmag3001_read_xyz_raw() is allowed.
ps_dev_tmag3001_get_game_direction() is forbidden.

ps_dev_ls013b7dh05_build_line_packet() is allowed.
ps_dev_ls013b7dh05_choose_dirty_regions() is owner policy and belongs above the device driver.
```

---

## Shared I2C3 Bus Policy

`I2C3` is shared by multiple Platform owners:

| Device | Address | Owner |
|---|---|---|
| `ADP5360` PMIC | `0x46` | `thPower` |
| `LIS2DUX12TR` IMU | `0x18` | `thSensor` |
| `TMAG3001A1YBGR` joystick | `0x34` | `thInput` |

HW6 uses a bounded shared-bus lease model.

```text
thPower  -> ADP5360 driver  \
thSensor -> LIS2DUX12 driver -> ps_hw_i2c3 lease layer -> STM32 HAL I2C3
thInput  -> TMAG3001 driver  /
```

Rules:

- `hi2c3` is private to `ps_hw_i2c3`.
- Device addresses are documented and passed as 7-bit addresses.
- STM32 HAL shifted-address handling is hidden inside `ps_hw_i2c3`.
- No I2C transaction may run from ISR context.
- Every transaction has an explicit timeout.
- Every multi-transaction sequence must use a bounded lease.
- No lease may be held across queue sends, thread sleeps, owner-state waits, or callbacks.
- No nested lease acquisition is allowed.
- Leases have a maximum duration.
- Failed, timed-out, or abandoned leases force bus recovery or fault publication.
- New leases are blocked during sleep quiesce.
- Resume must reinitialize or revalidate `I2C3` before device owners probe devices.

Conceptual API shape:

```c
ps_status_t ps_hw_i2c3_acquire(ps_hw_owner_t owner,
                               uint32_t acquire_timeout_ms,
                               uint32_t max_lease_ms,
                               ps_hw_i2c3_lease_t *lease);

ps_status_t ps_hw_i2c3_mem_read(ps_hw_i2c3_lease_t *lease,
                                uint8_t addr7,
                                uint16_t reg,
                                void *dst,
                                uint16_t len,
                                uint32_t timeout_ms);

ps_status_t ps_hw_i2c3_mem_write(ps_hw_i2c3_lease_t *lease,
                                 uint8_t addr7,
                                 uint16_t reg,
                                 const void *src,
                                 uint16_t len,
                                 uint32_t timeout_ms);

void ps_hw_i2c3_release(ps_hw_i2c3_lease_t *lease);
```

Simple one-shot helpers may acquire and release internally when no multi-register sequence is needed.

If measured contention, priority inversion, or async behavior later proves this model insufficient, the Platform may replace it with an owner-thread bus broker. Such a change requires this contract, [[RTOS_Ownership_and_Queue_Topology]], and affected device contracts to be updated first.

---

## Shared Bus Recovery

Shared bus recovery is centralized in the bus abstraction.

For `I2C3`, recovery must support:

- stuck-bus detection
- bounded recovery pulses or stop-condition generation where electrically valid
- peripheral reinitialization
- bounded retry count
- owner-visible error result
- health/fault publication through the affected owner

Device drivers may request recovery after a bus fault, but they must not implement independent bus-reset logic.

---

## DMA Policy

DMA paths are defined in [[HW6_DMA_Map]].

Rules:

- DMA handles are private to Platform hardware code.
- DMA start/abort/completion APIs are owned by the corresponding Platform owner path.
- DMA buffers must use documented memory regions and alignment.
- No clock-profile transition may occur while a DMA transfer is active.
- No STOP entry may occur while a critical DMA path is active.
- DMA callbacks may only capture completion/error status and signal the owner.
- Transfer completion and dirty-state commit happen in owner-thread context.

DMA buffer placement remains a documentation gap until the memory layout pass assigns regions and alignment.

---

## GPIO Safe Defaults

Safe defaults must be applied before owner threads can drive hardware.

Required HW6 defaults:

| Signal | Safe Default | Owner |
|---|---|---|
| display translator | hardwired enabled; no HW6 GPIO or software-disable state | hardware |
| `NINA_NRST` / `PC6` | low / BLE module reset asserted | `thComm` |
| `NINA_SW1` / `PC4` | high-Z analog/no-pull unless deliberately reconfigured | `thComm` |
| `NINA_SW2` / `PC5` | high-Z analog/no-pull unless deliberately reconfigured | `thComm` |
| `NINA_DTR` / `PC7` | high-Z analog/no-pull unless deliberately reconfigured | `thComm` |
| `NINA_DSR` / `PC8` | high-Z analog/no-pull unless deliberately reconfigured | `thComm` |
| `SD_MODE` / `PC9` | low / speaker amp shutdown | `thAudio` |
| `PWR_DBG` / `PH1` | low/idle; development-only marker to the battery-connector PPK2 logic input | Platform diagnostics |

Generated CubeMX output must not violate these defaults. If generated code does, the generated-code boundary or vault contracts must be corrected before firmware architecture work continues.

---

## IRQ And Callback Policy

ISRs and HAL callbacks may:

- capture minimal status required to avoid losing the event
- clear hardware flags when required by STM32 or device behavior
- enqueue fixed-size raw events where approved
- set ThreadX flags or wake owner threads

ISRs and HAL callbacks may not:

- run device protocols
- block
- busy-wait
- allocate memory
- call filesystem or storage APIs
- change clocks or sleep state
- publish Engine/Game events directly
- perform policy decisions

All ISR-originated events must be completed in the relevant owner-thread context.

---

## Timeout And Error Model

Every hardware abstraction operation must complete with a typed result.

Required result classes:

- success
- invalid argument
- invalid state
- busy
- timeout
- bus fault
- device NACK or no response
- DMA fault
- clock or timebase fault
- unsupported operation
- recovery required
- recovery exhausted

Raw STM32 HAL status values may be captured in diagnostics, but they must not leak as the primary cross-layer error model.

All timeouts must use documented timebase domains from [[Knobs_and_Tuning_Contract]].

---

## Sleep, Quiesce, And Resume

The hardware abstraction supports sleep transitions but does not decide sleep policy.

Rules:

- `thPower` owns sleep-class and clock-profile decisions.
- Owner threads quiesce hardware paths before `thPower` enters STOP or another low-power state.
- Hardware abstraction APIs must provide bounded abort, idle, or status-check helpers where the owner needs them.
- New transactions must be rejected once an owner enters quiesce.
- Resume must revalidate HAL and RTOS timebases before owners accept normal work.
- Shared buses and critical devices must be revalidated after wake before dependent operations continue.
- No resume path may assume a peripheral retained configuration unless measured evidence proves it for the selected sleep class.

---

## Bring-Up And Evidence Hooks

The hardware abstraction must make bring-up observable without destabilizing runtime behavior.

Required support:

- low-overhead trace points for state transitions, transfer start/done, faults, and recovery
- build-time option for additional bring-up diagnostics
- fault-injection hooks where practical
- device identity readback helpers
- ability to report the public 7-bit I2C address convention, timeout values, and clock profile used in a test

Bring-up behavior is not known-good until measured evidence is recorded in [[Brought_Up_Tracker]].

Temporary direct HAL/LL experiments must be recorded as temporary measures with exit criteria.

---

## Test And Fake Hardware Hooks

Where practical, Platform owner logic should be testable without real hardware by replacing hardware abstraction calls with fakes.

Rules:

- owner-thread state machines should not depend on raw STM32 handles
- hardware abstraction return values should be injectable in tests
- timeout, busy, NACK, DMA fault, and recovery-exhausted paths must be testable
- fakes must preserve ownership rules and bounded behavior

Test hooks must not weaken production determinism or allow Engine/Game code to bypass Platform owners.

---

## Required HW6 Abstraction Surfaces

The following surfaces are required before full Platform implementation:

| Surface | Primary Consumer | Notes |
|---|---|---|
| board safe GPIO defaults | boot supervisor | must enforce safe reset assumptions |
| clock/timebase validation | `thPower` | required before mode transitions complete |
| wake-source capture | `thPower`, boot/fault supervisor | unknown wake remains a defect |
| `I2C3` lease layer | `thPower`, `thSensor`, `thInput` | 7-bit addresses, bounded leases |
| display SPI/LPDMA path | `thDisplay` | display protocol driver remains policy-free |
| display GPIO/RTC support | `thDisplay`, `thPower` | hardwired translator assumption and `LCD_1HZ` coordination |
| `PWR_DBG` marker | Platform diagnostics | idle-low dev-only timing/power marker; packages have no access |
| audio SAI/GPDMA path | `thAudio` | 16 kHz mono baseline |
| OCTOSPI flash path | `thStorage` | bounded read/program/erase and deep power-down |
| USB device support | `thStorage`, `thPower` | installer/staging ownership only |
| button EXTI path | `thInput`, `thPower` | Start shipping overlay remains owner policy |
| BLE LPUART path | `thComm` | interrupt-driven rings first, DMA deferred |
| NINA control GPIO support | `thComm` | preserve high-Z strap/control defaults |

---

## Validation Cases

1. CubeMX handles are not referenced outside approved Platform hardware files.
2. Engine, package, Reference Game, and tool-facing code cannot include hardware abstraction headers.
3. safe GPIO defaults are applied before owner threads start.
4. `I2C3` accepts 7-bit device addresses and hides shifted STM32 HAL representation.
5. `I2C3` leases serialize PMIC, IMU, and joystick access.
6. `I2C3` lease timeout and abandoned-lease behavior route to recovery or fault publication.
7. no hardware abstraction API blocks indefinitely.
8. no ISR performs protocol parsing, storage work, clock changes, or policy decisions.
9. DMA transfer start/done/error callbacks signal owner context only.
10. quiesce blocks new transfers and drains or aborts active transfers with bounded timing.
11. resume revalidates clocks, timebases, buses, and device liveness before normal requests are accepted.
12. injected hardware faults produce typed errors and owner-visible health changes.
13. temporary direct HAL/LL bring-up experiments are recorded with exit criteria.

---

## Rule

The Platform hardware abstraction is boring, bounded, and policy-free.

It exists to make hardware access disciplined. It must not become a second Platform owner layer, and it must never become a game-facing API.
