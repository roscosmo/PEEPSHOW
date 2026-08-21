# Power and Sleep Policy

This document defines how the PeepShow Platform translates Engine and runtime intent into power behavior.

The package-facing time, cadence, lifecycle, wake, and power-intent API is defined in [[Time_And_Power_Intent_API_Contract]].

---

## Core Policy

- STOP-first strategy for reactive/event-wait operation.
- Reactive runtimes sleep as soon as the current bounded event transaction and its Engine actions settle; they do not remain awake waiting for an inactivity timeout.
- Power thread is sole owner of clock and sleep transitions.
- Runtime hosts provide intent, not hardware commands.
- No transition is complete until timebases are verified.
- PMIC, battery, charger, VBUS, and shipping-mode policy is owned by [[PMIC_and_Power_Contract]].
- Startup must respect the PMIC contract battery/VBUS gate before enabling display-intensive work, audio, vibration, radio, switched rails, package runtime, or installer behavior.

---

## Package Intent Boundary

Packages and runtime hosts may publish:

- system host, package scene type, and derived `REACTIVE`/`REALTIME` execution intent
- reactive wait contract and admitted event interests
- waiting-visual intent and fallback associated with a settled reactive state
- realtime cadence/frame budget and declared meaningful work
- latency tolerance and symbolic wake intents
- temporary capability context declarations
- declared inactive/end/failure routes
- interaction-state inactive route and meaningful-activity sources
- bounded inactivity deferral for declared non-interruptible work

Packages and runtime hosts must not publish or control:

- STOP level
- clock profile
- RTC programming
- LPBAM setup
- DMA/display transfer method
- peripheral power state
- PMIC policy
- wake-source electrical configuration

`thPower` may grant, clamp, delay, reject, shorten, or revoke any runtime request when Platform policy, battery state, sleep policy, or validated target profile requires it.

---

## Wake Reason Classification

All wakes must be classified as one of:
- RTC cadence
- button or joystick input
- sensor event
- PMIC interrupt
- USB attach/detach
- watchdog or fault recovery
- unknown (requires investigation)

Unknown wake reasons are defects until explained.

---

## Intent To Policy Mapping

Inputs to power manager:
- current execution semantic and pending bounded work
- reactive wait/event interests or realtime cadence budget
- allowed latency and symbolic wake intents
- waiting-visual grant/fallback result
- active capability contexts and owner blockers
- interaction state and admitted meaningful activity

Outputs from power manager:
- sleep class
- clock profile
- sensor mode
- input mode
- display transfer mode
- battery/charger policy action

When [[IMU_Contract|IMU]] step counting is active, power policy must select the deepest sleep class that preserves the IMU embedded functions and interrupt path. Absolute deepest sleep is invalid if it stops step counting or loses step-counter state.

---

## Runtime Policy Matrix

`SHELL`
- reactive by default
- sleeps whenever no system transaction is pending
- may use a Platform-owned waiting visual

`STATE_SCENE`
- `REACTIVE` event/schedule/state transactions
- no awake wait loop
- Platform may hold or autonomously animate the settled display while waiting

`SEQUENCE_SCENE`
- `REALTIME` bounded data-driven timeline execution
- explicit FPS/track/end bounds, meaningful-activity rules, suspend/resume behavior, and inactive route

`PROGRAM_SCENE`
- `REALTIME` bounded sandbox execution
- explicit instruction/memory/frame budgets, meaningful-activity rules, suspend/resume behavior, and inactive/failure routes

`INSTALLER`
- USB/staging ownership mode with strict subsystem isolation
- reactive by default; it is a Platform-owned host, not normal package gameplay

---

## Engine Execution Semantics

The Engine exposes execution semantics and game intent, not hardware power states.

| Semantic | Meaning |
|---|---|
| `REACTIVE` | Run a bounded event/state transaction, settle Engine actions and presentation, then yield until an admitted input, schedule, sensor, lifecycle, or system event occurs |
| `REALTIME` | Keep a sequence timeline or program scene frame-paced while that scene remains admitted |

`STATIC` is not an Engine execution-mode token. The word remains valid for static frames, static content, and internal one-shot display update classes where that terminology is accurate.

Waiting display behavior is attached to a settled reactive state. A state may request that the current frame be held or describe bounded visual motion while waiting. The package does not select LPBAM, STOP2, periodic MCU wakes, DMA layout, or SRAM placement. Platform display and power policy derive the backend from game-state intent, target-profile grants, compiled budget, wake requirements, and fallback behavior.

Package-visible concepts must not expose `STOP2`, `STOP3`, PLL settings, LPDMA, LPBAM, EXTCOMIN, PMIC registers, or wake-pin configuration.

---

## Active Operating-Point Policy

`REACTIVE` and `REALTIME` choose Platform policy objectives. They do not name clock frequencies, voltage scales, or literal operating points, and packages cannot request them.

| Execution semantic | Optimization objective | Admission condition |
|---|---|---|
| `REACTIVE` active burst | Minimize charge/energy from an admitted event through settled state, presentation preparation, owner quiesce, and return to the selected waiting backend | Meet reactive response-latency limits and complete the bounded transaction without owner, DMA, or timebase faults |
| `REALTIME` sustained execution | Minimize sustained power at an operating point that can satisfy worst-case frame, audio, sensor, and owner deadlines with margin | Meet target cadence, frame-budget, jitter, and audio-underrun limits for the admitted workload class |

A moderate or high reactive clock may use less total energy than a low clock if it shortens active time enough to return to STOP sooner. That is a race-to-sleep hypothesis to measure, not a default rule. Fixed-duration display, storage, radio, sensor, and audio operations may reduce or eliminate that benefit.

Realtime does not automatically mean the maximum clock. The selected point is the lowest measured point with adequate worst-case deadline margin. Audio sample/kernel clocks remain independently constrained and must not vary merely because CPU/SYSCLK policy changes.

Rules:

- `thPower` owns selection from measured internal operating points.
- Platform may define multiple internal points within one execution semantic when measurements justify workload classification, but packages still express only semantic, cadence, latency, and capability intent.
- clock/voltage changes occur only at owner-safe boundaries with no active DMA or bus transaction.
- voltage scaling, flash latency/cache state, SYSCLK/HCLK, and affected kernel clocks form one validated operating-point transition.
- transition latency and charge cost determine whether switching or hysteresis is worthwhile.
- if measurements do not justify dynamic switching, Platform uses one conservative validated point for that semantic.
- target profiles publish derived response, cadence, and estimate limits; they do not publish or accept package-selectable clock frequencies.

### Clock-Capability Resolution

`thPower` resolves clocks from owner state and capability requests. Owners ask for what they need to do, not for RCC register values or MHz.

Initial precedence for HW6:

1. STOP2, shipment, and forced-sleep preparation win once admitted: owners quiesce, active DMA/bus work drains, USB clock is disabled, and PLL2 is disabled unless a separately validated autonomous scenario owns it.
2. USB MSC/export or installer ownership requests `USB_DEVICE_ACTIVE`: USB `48 MHz` must be valid, STOP2 is blocked, and the internal policy should select the high I/O operating point instead of using an ad hoc USB-only clock override.
3. Storage/package/external-flash work requests `OCTOSPI_ACTIVE`: OCTOSPI kernel clocks stay valid until the transaction is idle, and SYSCLK/PLL changes are forbidden while the bus is active.
4. Audio playback requests `SAI_AUDIO_ACTIVE`: SAI kernel clocks remain stable for the sample rate, audio DMA is stopped before sleep, and CPU/SYSCLK policy must not disturb the audio clock.
5. `SEQUENCE_SCENE` and `PROGRAM_SCENE` work requests `REALTIME_DEADLINE_ACTIVE`: `thPower` selects the lowest measured realtime operating point with frame/audio/sensor/display margin.
6. `STATE_SCENE`, shell, menu, input, and bounded display work request `REACTIVE_TRANSACTION_ACTIVE`: `thPower` selects the lowest measured reactive point that meets response limits and returns to the selected waiting backend efficiently.

The first FW0 implementation is split between resolver scaffolding, one validated active USB path, one validated normal-boot cleanup path, a target-validated storage requester wrapper for MSC export/reclaim and explicit flash/staging provisioning, target-validated runtime requester hooks for bounded reactive transactions plus reactive/realtime package-admission scaffolds, a target-validated UI requester hook for bounded shell/router transactions, a target-validated display-transfer requester hook for the boot clear-hold transfer, and a no-sound audio requester scaffold for SAI clock ownership. FW0 reports requested capabilities, selected internal profile, blockers, requester slots, per-requester status, domain readback, and current profile status. Owners request capabilities; `thPower` resolves the profile, storage remains the owner of USB device hardware and MSC media sequencing, `thRuntime`/`thUI` publish symbolic execution intent only, `thDisplay` publishes bounded display-transfer intent only, and `thAudio` publishes SAI clock intent only while audio work genuinely needs the SAI kernel clock. Storage service boundaries use a named storage requester wrapper: MSC export/reclaim request `USB_DEVICE_ACTIVE | OCTOSPI_ACTIVE`, explicit flash/staging provisioning requests `OCTOSPI_ACTIVE`, and release clears the storage requester after cleanup. Runtime command boundaries use a named runtime requester wrapper: bounded shell/runtime transactions and reactive package stubs request `REACTIVE_TRANSACTION_ACTIVE` and release that requester slot after the transaction returns; realtime package stubs request `REALTIME_DEADLINE_ACTIVE` and hold that requester slot until runtime suspend or runtime return clears it, and runtime resume re-requests the saved realtime capability when returning to a suspended realtime unit. UI shell/router boundaries use a named UI requester wrapper: HOME dispatch, deferred router events, and input-driven UI actions request `REACTIVE_TRANSACTION_ACTIVE` and release after the bounded UI transaction returns. Display transfer boundaries use a named display requester wrapper: the validated FW0 boot clear-hold transfer requests `DISPLAY_TRANSFER_ACTIVE` and releases after the bounded display transaction completes; queued UI render calls are wrapped the same way but still need a settled HOME-render target capture. Audio boundaries use a named audio requester wrapper: reactive SFX may request `SAI_AUDIO_ACTIVE` for a bounded burst/drain window, realtime audio may hold it while admitted, and release clears the audio requester before STOP/LPBAM waiting can resume. Power shutdown boundaries now use the system-action admission layer before physical quiesce: START-shutdown, battery-critical, and boot-low-battery shipment prep may suspend an active runtime first; START-cancel is the only automatic resume path, and only for a runtime suspended by START-shutdown admission. STOP2 resident policy is per peripheral: physical quiesce may park a device differently for sleep without changing the logical active mode that higher layers requested. Flash/OCTOSPI and SAI/audio must be off or deep-sleep parked for STOP2; the baseline BLE resident state is NINA `SLEEP_SYSTEM_OFF` via the validated `AT&D4` / DSR path with reset released, while reset-low `RESET_HELD` is only an explicit comparison/fallback mode; TMAG3001 joystick is terminally parked in sleep for baseline STOP2 current work with the interrupt config kept as a policy-selected target for future wake-and-sleep validation; LIS2DUX12 defaults off, with step-counter STOP2 residency reserved until measured and validated.

STOP2 GPIO wake/park policy is staged. FW0 records a GPIO policy ledger before it changes pin modes: assigned pins are classified as used, wake-retained, hard-retained, or park candidates for ports A/B/C/H. Wake-retained pins are the validated or planned wake inputs from [[HW6_Wake_Sources]]; hard-retained pins cover debug, LSE/RTC/display timing, NINA sleep control, speaker shutdown, and the development power marker. Park candidates are non-wake peripheral pins that may be moved to analog/no-pull only after owner quiesce and target current evidence prove the change is safe. Parking is controlled by `power_stop2_gpio_park_group_mask` and debug override. Group bits are `0x1` OSPI, `0x2` SAI, `0x4` USB, `0x8` display SPI, and `0x10` I2C. The validated default is `0x1e`: park SAI, USB, display SPI, and I2C, while retaining OSPI pins. OSPI analog parking remains a diagnostic override only because HW6 PPK2 target evidence showed the OSPI group adds passive STOP2 current when moved to analog/high-Z. The STOP2 sequence snapshots GPIO `MODER`, `PUPDR`, `ODR`, and `IDR` before parking, during the parked sleep window, and after wake restore. Sensor terminal sleep writes must complete before the I2C group may be parked, because TMAG3001 I2C traffic can wake the sensor from sleep; future TMAG wake-and-sleep residency must use a separate owner-selected policy rather than this baseline deep-sleep parking path.

STOP2 entry boundaries are now split into explicit decision and entry steps. `thPower` first runs the STOP2 eligibility dry-run and records the blocker mask without sleeping; the controlled-entry helper may enter STOP2 only when that ledger is clear, then reuses the validated owner-quiesce, STOP2, physical START wake, clock-restore, and owner-recovery scaffold. HW6 STOP2 keeps all SRAM banks powered/retained; the sleep path must not depend on selective SRAM bank shutdown, pre-sleep copying into a special retained bank, or post-wake reconstruction of ordinary RAM state. The automatic idle admission path is enabled by default for the held-frame baseline: `thPower` periodically checks the same STOP2 ledger plus runtime, UI/router, display-stable state, storage/USB, input, owner-queue, and required-idle-window conditions before calling controlled entry. Display-stable state means the display owner reports a completed, successful operation whose displayed page matches the UI router. FW0 now separates display waiting into selected backends: `HELD_FRAME` is the default baseline backend for STOP2/current bring-up, and treats a stable current-page frame as display-ready without arming LPBAM; if an awake renderer animation such as the cursor blink is active, `thPower` suppresses it and asks `thDisplay` to present one cursor-visible held frame before entry so STOP2 does not inherit a mid-blink image or keep the blink scheduler alive. `LPBAM` remains the autonomous-display backend and must be selected only by knob/test/profile policy until HW6 payloads are visually validated, current-measured, and integrated with renderer dirty-row tracking. LPBAM display residency is a separate display-owned readiness signal: normal display updates clear `display_lpbam_ready`, `thPower` must request an LPBAM prepare handoff from `thDisplay`, and `thDisplay` may satisfy STOP2 LPBAM validation only after a real autonomous handoff is prepared and the ready page/status/render-count still match the current UI state. When the LPBAM backend is selected but no validated LPBAM slice exists for the current renderer state or target profile, `thDisplay` intentionally answers `UNAVAILABLE` and `thPower` records the missing readiness as a hard `LPBAM_NOT_READY` blocker. If a successful LPBAM prepare is followed by a late hard blocker, input, UI/router, display, storage, or queue change before entry, `thPower` must abort the display LPBAM handoff before refusing entry. FW0 has target-validated both the older debug-only forced-ready late-abort shape and the first real cursor-slice LPBAM prepare/start/STOP2/wake/abort path. If any hard blocker is present, the helper refuses entry and records the blocker instead of sleeping.
FW0 now treats STOP2 as an idle-residency target, not a late parking operation. After normal boot has brought up the shell/runtime and completed boot power cleanup, `thPower` sends one bounded boot-idle park command to `thComm` and `thSensor`: NINA enters DSR `SLEEP_SYSTEM_OFF` and LIS enters deep-power-down. Later STOP2 eligibility records `stop2_eligibility_idle_peripheral_park_ready`; if BLE/LIS are not already in their resident states, `IDLE_PARK` (`0x4000`) blocks STOP2 instead of using STOP2 entry to do first-time module bring-up. Owner quiesce still handles deliberate active modes through per-owner policy, so TMAG wake-and-sleep and LIS step-counter STOP2 states remain possible once separately measured and validated.

HW6 evidence `EV-HW6-20260812-P1-CLOCKUSB-037` validates the active USB handoff: MSC export requests `USB_DEVICE_ACTIVE`, `thPower` applies the `CLK_IO_HIGH` path, the host mounts the staging FAT volume, and reclaim closes FileX/LevelX, releases the storage requester slot, returns to the base `24 MHz` profile, and disables USB clock/VDDUSB/HSI48.

HW6 evidence `EV-HW6-20260812-P1-CLOCKBOOT-038` validates the normal-boot cleanup path: after CubeMX/generated boot leaves USB PCD/VDDUSB state present, `thPower` sends a short storage-owned USB boot-park command. `thStorage` only parks USB device hardware, refreshes clock readback, and ACKs; it does not run full storage/flash initialization, FileX/LevelX, or MSC export. Follow-up evidence `EV-HW6-20260813-P1-USBOOTFIX-049` validates the inactive boot-park fix: when MSC was never active, USB parking no longer calls HAL active-device disconnect/stop/deinit or FIFO flush. The validated boot result has HOME rendered, USB PCD reset, USB clock/VDDUSB/HSI48 disabled, and no long storage OSPI action, while menu-driven MSC export/reclaim still mounts, transfers, reclaims, and returns to firmware ownership.

HW6 evidence `EV-HW6-20260812-P1-CLOCKSTORAGE-039` validates the named storage requester wrapper for MSC export/reclaim. During active MSC, the storage requester slot held `USB_DEVICE_ACTIVE | OCTOSPI_ACTIVE` (`ST=0x3`), the selected/current profile was `CLK_IO_HIGH`, required/managed/readback domains were `0x3/0x3/0x3`, and STOP2 was blocked. After safe eject/detach and reclaim, the storage requester released to `ST=0x0`, selected/current returned to `REACTIVE_BASE`, required/managed/readback domains were `0x0/0x0/0x0`, STOP2 was ready, USB clock/VDDUSB/HSI48 were off, and PLL2 was off.

HW6 evidence `EV-HW6-20260812-P1-FLASHINIT-040` validates explicit flash/staging provisioning through the same storage requester wrapper. The destructive flash init command completed with flash init status `0x0`, wake/layout/FileX-LevelX/deep-power-down statuses `0x0/0x0/0x0/0x0`, erased `1280` blocks in the USB staging/export region, formatted/opened/flushed/closed FileX with all statuses `0x0`, and mounted as an empty host volume afterwards. Clock policy returned to `REACTIVE_BASE`, storage clock flash/release statuses were `0x0/0x0`, STOP2 was ready, domains were clear, USB/VDDUSB/HSI48 were off, and PLL2 was off after release.

HW6 evidence `EV-HW6-20260814-P1-STORAGEATTACH-063` validates non-destructive storage attach after reset. The attach helper queued a storage-owned check without erase or format, returned status `0x0`, restored storage state to `STORAGE_FLASH_READY` and flash state to `FLASH_DEEP_POWER_DOWN` (`2/8`), read JEDEC `1f 42 18` with match `1`, validated layout region `10`, entered deep power-down with status `0x0`, and parked OCTOSPI clocks (`ENR1/2 0xc020008f/0x10 -> 0xc000008f/0x0`). Destructive flash init remains only for provisioning or explicit recovery; normal reset recovery must use non-destructive attach.

HW6 evidence `EV-HW6-20260813-P1-RUNTIME-044` validates the first runtime-class scaffold against power-policy terminology. Normal shell boot reports `SHELL / REACTIVE / RUNNING`; package transfer enters `INSTALLER / REACTIVE / RUNNING`; the valid-package prompt remains in `INSTALLER`; and install-stub completion returns to `SHELL`. This evidence validates naming and lifecycle plumbing only. It does not validate measured reactive current, realtime admission, package execution, or final installer commit behavior.

HW6 evidence `EV-HW6-20260813-P1-RUNTIMECLOCK-045` validates the first runtime-to-power requester link. After boot, `thRuntime` reported one reactive clock request and one release (`runtime clock req/rel = 1/1`), reactive and release statuses were `0x0`, the runtime requester slot `RT` settled back to `0x0`, per-requester status reported `RT=0x0`, `STOP2 ready` remained `1`, and USB/VDDUSB/HSI48 plus PLL2 were off in idle. This proves the OS plumbing for runtime clock intent and requester-specific ACK/status handling; it does not prove the measured reactive operating point, realtime admission, package execution, or final scheduling policy.

HW6 evidence `EV-HW6-20260813-P1-RUNTIMEADMIT-050` validates the first runtime-admission power behavior. Breakpoint-based GDB validation stopped after each `thRuntime` command completed. Reactive package-stub admission reported class/exec/lifecycle `LP_MODULE / REACTIVE / RUNNING` (`3/1/2`), admission caps `0x20`, runtime request/release `2/2`, requester cap `RT=0x0`, and `STOP2 ready=1`. Realtime package-stub admission reported `RT_SCENE / REALTIME / RUNNING` (`4/2/2`), admission caps `0x10`, runtime request/release `3/2`, requester cap `RT=0x10`, active capabilities `0x10`, and `STOP2 ready=0`. Runtime return reported `SHELL / REACTIVE / RUNNING`, runtime request/release `4/3`, requester caps clear, and `STOP2 ready=1`. This validates symbolic reactive-vs-realtime admission and release behavior only; it does not validate real package execution, renderer/audio load, measured current, or the final realtime operating point.

HW6 evidence `EV-HW6-20260813-P1-RUNTIMESUSPEND-051` validates the first runtime suspend/resume power behavior. Breakpoint-based GDB validation stopped after each `thRuntime` command completed. Realtime package-stub admission reported `RT_SCENE / REALTIME / RUNNING`, runtime caps `0x10`, requester cap `RT=0x10`, and `STOP2 ready=0`. Runtime suspend reported lifecycle `SUSPENDED` (`3`), saved class/exec/lifecycle/caps `4/2/2/0x10`, suspend release status `0x0`, runtime active caps `0x0`, requester caps clear, and `STOP2 ready=1`. Runtime resume reported lifecycle `RUNNING` (`2`), resume request status `0x0`, runtime active caps `0x10`, requester cap `RT=0x10`, and `STOP2 ready=0`. Runtime return then cleared active caps and requester caps and returned `STOP2 ready=1`. This validates lifecycle-driven realtime power intent only; it does not validate real package pause/resume callbacks, renderer/audio workload restart, measured current, or the final realtime operating point.

HW6 evidence `EV-HW6-20260814-P1-POWERADMIT-054` validates that power-owned shutdown prep enters through the same system-action admission layer before owner quiesce. A reactive package stub was admitted as `LP_MODULE / REACTIVE / RUNNING`; START-shutdown admission then reported action/result/reason/status `4 / 2 / 3 / 0x0`, moved `thRuntime` to lifecycle `SUSPENDED`, and marked the suspension as system-owned action `4`. A START-cancel dry-run then resumed only that START-suspended runtime, cleared the system-owned suspend marker, and returned `thRuntime` to lifecycle `RUNNING` with suspend/resume counts `1 / 1`. This proves the power-admission scaffold and cancel resume hook only; final save/quiesce content, package callbacks, low-battery software-shipment enablement, and measured current remain open.

HW6 evidence `EV-HW6-20260813-P1-STOP2ELIG-052` validates the first production-shaped STOP2 eligibility dry-run. The dry-run is owned by `thPower`, does not enter STOP2, and records whether STOP2 would be allowed now plus the plain blocker mask. Baseline shell reported ready `1`, blockers `0x0`, and pending pre-entry work `0x3` (`OWNER_QUIESCE | LPBAM_VALIDATION`). Realtime runtime admission reported ready `0` with blocker `CLOCK_CAP` (`0x10`) because `REALTIME_DEADLINE_ACTIVE` was held by the runtime requester. Runtime suspend then cleared the requester caps and returned ready `1`. This validates the decision ledger and blocker mapping only; real automatic STOP entry, wake classification, tick compensation, LPBAM visuals, current, repeated cycles, and fault injection remain open.

HW6 evidence `EV-HW6-20260813-P1-STOP2CTRL-053` validates the first default-off controlled STOP2 entry helper. The helper was manually queued for `thPower`, ran the eligibility ledger first, reported eligibility status/block/pending `0x0/0x0/0x3`, attempted entry once with status `0x0`, incremented owner STOP2 count from `0` to `1`, entered real STOP2, returned from one physical START wake, and reported owner quiesce/enter/clock/recover/last statuses all `0x0`. This validates the production-shaped manual admission path plus the already validated STOP2 mechanics.

HW6 evidence `EV-HW6-20260814-P1-STOP2WAKE-056` validates controlled STOP2 START wake classification with debug-low-power explicitly disabled. A first diagnostic run with `DBGMCU->CR = 0x6` (`DBG_STOP | DBG_STANDBY`) entered STOP2 but returned one tick later with no PA4 change, no button edge delta, no EXTI pending, no NVIC pending/active interrupt, no PMIC edge, and no PWR wake flag, so it was correctly classified as `UNKNOWN`. The corrected helper cleared `DBGMCU->CR` at `0xE0044004` to `0x0`; the next run reported `DBGMCU CR before/after = 0x0/0x0`, owner STOP2 count `0 -> 1`, `PWR_SR.STOPF = 0x2`, source mask `0x1`, primary cause `START`, PA4 `IDR 0x6055 -> 0x6045`, START button edges `0 -> 1`, no unknown wake count, and no PMIC/joystick/sensor/USB evidence. This validates the controlled helper's physical START-wake classification path and establishes that STOP2 measurements must state debug-low-power state. All other wake sources, LPBAM-ready automatic entry, tick compensation, LPBAM waiting visuals, current, repeated cycles, and fault injection remain open.

HW6 evidence `EV-HW6-20260814-P1-STOP2AUTO-055` validates the default-off automatic idle dry-run after a settled menu render: automatic entry stayed disabled (`enabled/check/entry/skip = 0/1/0/1`), no real STOP2 attempt occurred (`owner stop2 count = 0`), the display false-blocker was cleared (`auto block = 0x0`), the display owner reported stable current-page state (`display complete/success = 1/1`, UI page/display page `2/2`), and the remaining pending mask `0x7` was expected for a non-entering dry-run (`OWNER_QUIESCE | LPBAM | IDLE_WINDOW`). HW6 evidence `EV-HW6-20260814-P1-STOP2AUTO-LPBAM-057` validates the knob-enabled automatic idle path's conservative LPBAM block: `power_auto_stop2_enable` was compiled true, probe API `27` reported `auto enabled/check/entry/skip = 1/9/0/9`, blocker mask `0x2000`, pending mask `0x3`, eligibility still clear (`elig ready/block/pending = 1/0x0/0x3`), `lpbam_stop2_ready = 0`, and owner STOP2 count stayed `0`. HW6 evidence `EV-HW6-20260814-P1-DISPLPBAM-058` validates the first display-owned LPBAM readiness scaffold: probe API `28` reported display stable (`display complete/success = 1/1`, display/UI page `2/2`) while `display_lpbam_ready = 0`, ready page/status `0xffffffff/0xffffffff`, clear count/reason `3/3`, and auto pending `0x7`; owner STOP2 count stayed `0`. This proves display stability and LPBAM readiness are separate states, and `thPower` now consumes the display-owned readiness result instead of a clock-policy placeholder. HW6 evidence `EV-HW6-20260814-P1-LPBAMHANDSHAKE-059` validates the first bounded LPBAM prepare owner handoff: probe API `29/14` reported a `thPower -> thDisplay` prepare request with send/wait statuses `0x0/0x0`, ACK flags `0x108` including the display owner ACK bit `0x8`, display prepare status `0xfffffffe` (`UNAVAILABLE`), display ready `0`, and owner/ready/clear `0xfffffffe/0/2`. This is the correct blocked result until real LPBAM display payload preparation exists. HW6 evidence `EV-HW6-20260814-P1-LPBAMABORT-060` validates the late-abort safety path for a future successful prepare: a debug-only forced-ready handoff reported display prepare status `0x0`, ready `1`, prepare count `1`, then a synthetic late input blocker produced final block `0x2800` (`INPUT | LPBAM_NOT_READY`), display abort status `0x0`, late blocker count `1`, and owner STOP2 count stayed `0`. LPBAM-ready automatic STOP2 entry, real autonomous payload preparation, tick compensation, LPBAM waiting visuals, current, repeated cycles, and fault injection remain open.

HW6 evidence `EV-HW6-20260814-P1-STOP2HELD-061` validates the default held-frame display wait backend for automatic STOP2 idle dry-run. Probe API `31` reported `display backend req/selected/status/held = 1/1/0x0/1`, stable display state (`display complete/success = 1/1`, UI/display page `2/2`), LPBAM still unavailable (`display_lpbam_ready = 0`, status `0xffffffff`), no hard blockers (`auto block = 0x0`), eligibility clear (`elig ready/block/pending = 1/0x0/0x1`), and dry-run-only pending work `0x5` (`OWNER_QUIESCE | IDLE_WINDOW`). No real STOP2 entry occurred (`owner stop2 count = 0`) because this was the default-off dry-run helper. This validates that baseline STOP2/current work can use a held frame without treating missing LPBAM payloads as a blocker; it does not validate real automatic STOP2 entry, measured current, LPBAM playback, or repeated sleep/wake cycles.

HW6 evidence `EV-HW6-20260814-P1-STOP2AUTOENTRY-062` validates the explicit one-shot held-frame automatic STOP2 entry helper. The helper first cleared debug-low-power (`DBGMCU CR before/after = 0x0/0x0`), then forced only this debug request past the compile-time auto-enable knob and treated the idle window as already satisfied. Probe API `32` reported `auto enabled/check/entry/skip = 0/1/1/0`, `auto force enable/count/tick = 1/1/359`, `auto status = 0x0`, `auto idle start/live/required ticks = 159/200/200`, no blockers (`auto block = 0x0`), held-frame backend ready (`display backend req/selected/status/held = 1/1/0x0/1`), and owner STOP2 count `0 -> 1`. The controlled-entry print reported quiesce/enter/clock/recover/last all `0x0`, `PWR_SR.STOPF = 0x2`, PA4 IDR `0x6055 -> 0x6045`, START edges `0 -> 1`, wake source/primary `0x1/1`, one START wake count, and zero unknown/PMIC/joystick/sensor/USB wake counts. This validates real STOP2 entry and START wake/resume through the held-frame auto-admission path; it does not validate production periodic auto-sleep policy, measured current, LPBAM playback, non-START wake sources, or repeated cycles.

HW6 evidence `EV-HW6-20260816-P1-STOP2AUTO-PERIODIC-067` validates the first default-on periodic automatic STOP2 entry under the held-frame backend. After `power_auto_stop2_enable` was compiled true, `thPower` reported `auto enabled/check/entry/skip = 1/5/1/4`, held-frame backend ready (`display backend req/selected/status/held = 1/1/0x0/1`), owner STOP2 count `0 -> 1`, clean quiesce masks `0x7e/0x7e/0x7e/0x7e/0x0`, SysTick disabled during sleep (`0x10007 -> 0x4 -> 0x7`), START wake source/primary `0x1/1`, and zero unknown/PMIC/joystick/sensor/USB wake counts. The user also observed the unit re-enter STOP2 after wake. This validates the default-on held-frame periodic entry path; exact second-entry counters, LPBAM playback, non-START wake sources, and fault-injection behavior remain open.

HW6 evidence `EV-HW6-20260814-P1-STOP2STORAGE-064` validates STOP2 wake with storage attached and flash parked in deep power-down. After non-destructive attach, controlled STOP2 entry reported status `0x0`, owner STOP2 count `0 -> 1`, START wake source/primary `0x1/1`, and owner quiesce/enter/clock/recover/last all `0x0`. The post-STOP storage path requested and released `OCTOSPI_ACTIVE` internally from `thPower` (`storage clock post/release/status = 0x0/0x0/0x0`) before flash revalidation; storage power release and JEDEC checks both returned `0x0`, JEDEC match was `1`, and OCTOSPI clocks were restored for the flash check then parked again. This fixes the earlier failure where post-wake flash revalidation ran while OCTOSPI/PLL2 clocks were off. It does not validate measured current, repeated sleep/wake cycling, or non-START wake sources.

HW6 evidence `EV-HW6-20260816-P1-STOP2REENTRY-066` validates repeated controlled STOP2 entry after the TMAG3001 terminal-sleep proof became idempotent. The second request reported control count/status `2/0x0`, entry attempts/status `2/0x0`, owner STOP2 count `1 -> 2`, quiesce failure mask `0x0`, all owner quiesce statuses `0x0`, and joystick terminal-sleep proof `0x0/1/1`, so `thPower` no longer re-touches the already-slept TMAG on the next STOP2 pass. This validates repeated controlled entry/resume; production periodic automatic cycling still needs direct target evidence.

HW6 evidence `EV-HW6-20260816-P1-STOP2FLOOR-065` accepts the current baseline STOP2 floor for FW0 bring-up before LPBAM payload work. With SWD/debugger detached for measurement, controlled STOP2 entered and woke through physical START with `DBGMCU->CR = 0x0`, SysTick disabled during sleep (`CTRL 0x10007 -> 0x4 -> 0x7`), NINA in validated DSR `SLEEP_SYSTEM_OFF` with reset released, TMAG3001 terminal sleep committed with no post-sleep I2C read, LIS2DUX12 deep-power-down committed, SAI/audio shut down, and GPIO parking set to validated mask `0x1e` (`SAI | USB | display SPI | I2C`, OSPI retained). PPK2 target evidence measured approximately `50 uA` STOP2 average after the TMAG/LIS/NINA sleep and GPIO policy fixes. A 30-minute PPK2 summary (`ppk-20260816T024401_per_second_summary.csv`) showed seconds `4-1800` at `51.39 uA` mean, `50.87 uA` median per-second mean, and no STOP2 second above `66.22 uA` average; remaining short pulse clusters recur roughly every three minutes and are treated as known residual board/module activity until physical isolation or board revision work can identify them. OSPI analog parking remains excluded from the default because target evidence showed it adds roughly `200 uA` passive STOP2 current. This evidence validates the baseline held-frame STOP2 current target for proceeding to LPBAM; it does not validate LPBAM display playback current, repeated sleep/wake cycling, or sub-50 uA residual-pulse root cause.
HW6 evidence `EV-HW6-20260816-P1-STOP2IDLEPARK-068` validates boot-idle parking and fast repeated automatic STOP2 entry after probe API `36`. The boot-idle park ran once after boot power cleanup (`done/count/status/start/end = 1/1/0x0/27/255`) and received bounded ACKs from `thComm` and `thSensor` (`BLE send/wait/ack = 0x0/0x0/0x40`, `IMU send/wait/ack = 0x0/0x0/0x10`). Automatic STOP2 later reported `enabled/check/entry/skip = 1/5/2/3`, no hard blockers, `stop2_eligibility_idle_peripheral_park_ready = 1`, owner STOP2 count `1 -> 2`, and owner quiesce start/end on the second entry at the same tick (`663/663`). This confirms the slow BLE/LIS first-time parking cost has moved to boot-idle, and repeated STOP2 entry no longer performs that setup in the entry call. Final production wake-source policy and production LPBAM visuals remained open at this checkpoint.

HW6 evidence `EV-HW6-20260816-P1-BUTTONWAKE-LIST-069` validates A/B/L/R as automatic held-frame STOP2 wake sources while preserving the wake press as normal input. The STOP2 entry path now clears stale pending EXTI/NVIC state for the START+A/B/L/R wake set before WFI and does not clear A/B/L/R after wake. Target evidence after repeated button wakes reported `auto enabled/check/entry/skip = 1/28/9/19`, no hard blockers (`auto block = 0x0`, `elig ready/block/pending = 1/0x0/0x1`), owner STOP2 count `9`, wake source/primary `0x2/2` (`BUTTON`), button wake count `9`, button edges `22 -> 23`, input logical/policy counts `12/12`, and display completion/held-frame readiness still good (`display complete/success = 1/1`, backend `1/1/0x0/1`). Brief L/R presses visibly woke the MCU, moved menu focus, and returned to STOP2 on allowed shell pages. Calibration page `4` intentionally remained a UI auto-idle blocker in this evidence.

HW6 evidence `EV-HW6-20260816-P1-LPBAMCURSOR-070` validates the first real LPBAM display backend path through automatic STOP2 admission. With the debug/test LPBAM backend override set to `2`, `thPower` requested LPBAM prepare from `thDisplay`, the display owner built the current cursor-slice payload (`cursor row/count/col/count = 153/8/73/16`, `payload frames/chunks/bytes = 4/4/732`), enabled autonomous SPI3/LPTIM1/LPDMA1/SRAM4 clocks, linked a circular `24`-node LPDMA queue, and started LPTIM1 with `ARR/CMP = 7812/3906`. Target evidence reported all fill/clock/link/start and abort/reclaim statuses as `0x0`, STOP2 blockers clear, owner STOP2 count `2`, and the user visually observed autonomous animation during STOP2. The animation was not visually correct yet: the cursor rows did not blink in unison. This evidence validates LPBAM ownership, SRAM4 placement, queue execution, STOP2 entry, wake, and reclaim; it does not validate production visual quality, renderer dirty-row integration, LPBAM active current, or final waiting-animation policy.

HW6 evidence `EV-HW6-20260817-P1-AWAKEBLINK-072` validates awake cursor blinking plus held-frame STOP2 handoff after RTOS probe API `38` and owner probe API `20`. The cursor blink ran visibly while awake on HOME, then idle admission suppressed the blink and sent a bounded `thDisplay` cursor-visible request before STOP2. Target evidence reported blink request/render `14/14`, dirty rows `8/153/160`, handoff count/status/send/wait/ack/owner `1/0x0/0x0/0x0/0x108/0x0`, no hard blockers, and owner STOP2 `1/594/594/594`. This proves awake diagnostic animation no longer keeps the system awake for the held-frame backend; it does not validate LPBAM visual correctness or active LPBAM current.

HW6 evidence `EV-HW6-20260818-P1-LPBAMCURSOR32-073` validates complete-cursor LPBAM animation through repeated automatic STOP2 entry, wake, reclaim, and re-entry. RTOS/owner probe APIs `42/24` selected backend `2`, compiled `4/4/732` frames/chunks/bytes for dirty rows `8/153/160`, linked a `28`-node circular LPDMA queue, used SPI3 FIFO threshold `4`, and reported all fill/clock/link/start plus abort/reclaim statuses `0x0`. Owner STOP2 count reached `2`, no diagnostic clock override was active, and the cursor visibly blinked as one complete region both awake and in STOP2. This closes the autonomous cursor transfer framework and repeatable ownership lifecycle; active LPBAM current and general production waiting visuals remain open.

HW6 evidence `EV-HW6-20260820-P1-LPBAMREGEN-074` validates the same LPBAM/STOP2 behavior after moving runtime queue construction and required DMA-node compatibility fixes out of CubeMX-generated sources. A clean post-regeneration build retained `15992` SRAM4 bytes. Target probe APIs `48/24` reported prepare/commit/abort counts `5/5/5`, all queue and reclaim statuses `0x0`, owner STOP2 count `5`, automatic checks/entries `25/5`, display-clock release rechecks `10`, and cursor-blink idle-window preservation count `6`. The handoff prepared at tick `1373`, committed at `1374`, and reached WFI at `1374`, closing the earlier `25`-tick power-owner scheduling delay. At this checkpoint a small visual handoff hitch remained open, along with LPBAM active-current measurement and general production waiting visuals.

HW6 evidence `EV-HW6-20260820-P1-REACTIVELPBAM-075` validates the final input-safe cursor handoff for this bring-up unit. Probe APIs `49/10` reported raw A/B/L/R ISR send/process/drop `32/32/0`, queue enqueue/dequeue/drop `32/32/0`, and logical press/policy delivery/UI action `16/16/16`. The final pre-WFI check ran three times with `veto/status = 0/0x0`; its successful snapshot had enqueue/dequeue `4/4`, queue depth `0`, inactive live GPIO levels, and all button FSMs released. Automatic and owner STOP2 counts both reached `3`, and the user could not perceive the transition between awake and autonomous cursor playback. A later snapshot showing blocker `0x2000` with pending `0x3` represents the next bounded LPBAM prepare handoff between successful entries, not a failed prior entry. This closes the visible cursor handoff and tested ordered-input race. LPBAM active-current measurement, general production waiting visuals, and unimplemented long/repeat/chord/stuck input remain open.

HW6 evidence `EV-HW6-20260820-P1-LPBAMCURRENT-076` validates the first LPBAM STOP2 current baseline with real low-power debug conditions. Debug-in-low-power was disabled, SWD was detached, and the target averaged approximately `56 uA` over five minutes while the `250 ms` cursor animation ran autonomously. Compared with the prior held-frame STOP2 mean of `51.39 uA` from `EV-HW6-20260816-P1-STOP2FLOOR-065`, the observed average increase is approximately `4.61 uA` or `9%`. At the four-transfer-per-second cursor cadence, this is roughly `1.15 uC` per transfer if both captures are otherwise comparable. This closes average-current validation for the current eight-row cursor payload. Transfer-pulse shape, paired same-session baseline comparison, and scaling across production cadence, row, and chunk limits remain open.

HW6 evidence `EV-HW6-20260820-P1-WAITANIM-077` validates repeated STOP2 admission with the first bounded renderer-owned waiting-animation descriptor. Target probe APIs `50/25` reported program phases/frames/cadence `2/4/250`, sequence `0/1/0/1`, `8` candidate native rows, payload `4/4/572`, queue nodes `28`, and all fill/clock/link/start and abort statuses `0x0`. Handoff request/run counts converged at `7/7`; prepare count reached `7`, commit and owner STOP2 counts reached `6/6`, and the user observed responsive input after many presses at varied handoff timings. A transiently invalid display handoff is now cancelled rather than reused with stale page/render metadata, and an explicit rearm latch asks `thPower` for a fresh full admission check when display eligibility returns. This successful stress run did not exercise that fallback (`rearm pending/count = 0/0`), so controlled rearm fault injection remains open. The renderer descriptor is static and deterministic, and the display-owner prepare stack frame was reduced from `96` to `40` bytes after a target `STKOF` HardFault exposed insufficient stack headroom; no thread stack budget, LPBAM SRAM4 allocation, clock, or STOP2 hardware policy changed.

HW6 evidence `EV-HW6-20260820-P1-DMASRAMSPLIT-078` validates the display DMA/memory split across STOP2 wake. Awake display staging moved to normal SRAM and SPI3 uses GPDMA1 channel 0; LPBAM retains SRAM4 payloads/descriptors and explicitly operated LPDMA1 channel 0. SRAM4 build use fell from `15992` to `12632` bytes. The first wake test exposed that CubeMX MSP initialization links LPDMA last, causing the first awake render to fail when `hspi3.hdmatx` was not restored. The corrected wake path relinks GPDMA immediately after SPI reinitialization; target evidence then reported post-wake SPI/GPDMA handle equality, distinct LPDMA ownership, awake DMA error `0x0`, and display completion/success/status `1/1/0x0`. STOP2 admission ownership, SRAM retention policy, and LPBAM autonomous-clock policy are unchanged.

HW6 evidence `EV-HW6-20260821-P1-LPBAMPOOL-079` validates the bounded LPBAM resource check that precedes autonomous-display clock enable and STOP2 admission. The compiler exposes fixed capacities of four sequence entries, `12` shared chunks, and `9216` payload bytes, with explicit failure reasons for each bound. The unchanged cursor program passed with status/reason `0x0/0`, sequence `4/4`, chunks `4/12`, and payload `575/9216` bytes, then retained the previously validated wake and STOP2 behavior. The remaining `3816` bytes of SRAM4 linker capacity are reserved headroom and do not enlarge the admitted authored-animation payload budget unless the Platform allocation is deliberately revised and revalidated.

HW6 evidence `EV-HW6-20260821-P1-LPBAMCOMPOSITE-080` validates that one `250 ms` cadence edge gates one complete logical display frame even when that frame requires several bounded Sharp transactions. A two-phase full-panel test compiled `168` dirty rows per step into four `983/983/983/503`-byte transactions and changed the whole panel black/white without a visible sweep. The queue used `56` nodes, timer-triggered only transactions `0` and `4`, and kept SPI autonomous trigger gating disabled (`AUTOCR/TRIGEN = 0x60000/0`) so continuation transactions ran immediately. A separate composite test combined a two-phase cursor with a four-phase bar into four sequence steps; admission API `3` reported `4/12` steps, `8/12` chunks, and `7144/7151/9216` wire/used/capacity bytes, and both elements animated together correctly in STOP2. This closes coherent multi-chunk logical frames and bounded multi-element phase composition. Production authoring/package integration, fallback policy evidence, large-payload current scaling, and display fault recovery remain open.

HW6 evidence `EV-HW6-20260821-P1-LPBAMHANDOFF-081` validates the production waiting-animation handoff after the general compiler and coherent transport work. Probe APIs `52/26` move payload compilation and LPDMA queue construction into an early `thDisplay` preparation step, while the selected `1 -> 0` edge performs only the final phase render, hardware prearm, commit, and immediate power-owner recheck. The final target capture retained program phases/frames/cadence `2/4/250`, sequence `0/1/0/1`, payload `4/4/572`, four `143`-byte transactions, and owner STOP2 count `2`. LPTIM1 `/128` reported `ARR/CMP=7813/7812`; this keeps recurring phase intervals at `250 ms` and places the first autonomous next-phase edge at the end of the initial interval. The user reported a seamless awake-to-STOP2 transition. UI/page changes still invalidate prepared data, and STOP2 remains blocked until the edge-time prearm reports current-page/current-render readiness.

HW6 evidence `EV-HW6-20260821-P1-LPBAMFALLBACK-082` validates bounded held-frame fallback after explicit LPBAM compiler rejection. A four-step full-panel diagnostic exceeded the fixed payload budget and reported admission API/status/reason `3/0x1/4`, sequence `2/12`, chunks `10/12`, and payload `8879/9216` bytes. Probe APIs `52/27` then resolved requested/selected backend `2/1`, reported backend status/held readiness `0x0/1`, kept LPBAM ready/active `0/0`, and entered STOP2 twice with owner status `0x0`. The panel held a solid non-blinking cursor, proving the rejected payload was neither partially armed nor allowed to keep the CPU awake. Cosmetic blink renders preserve the explicit rejection long enough for `thPower` to consume it; UI/page changes clear it and require fresh admission.

The power owner must preserve accumulated idle time only for the bounded cursor-blink display-clock transaction that participates in the selected LPBAM edge handoff. Other transient blockers still reset the idle window normally. The validated `4 MHz` MSIK, LPTIM `/128`, `ARR/CMP=7813/7812`, and normal APB3 `/8` policy must remain unchanged unless new target evidence contradicts them.

After LPBAM commit and owner quiesce, `thPower` must perform one final atomic input admission check immediately before WFI with interrupts masked. All owner queues must be empty, raw input enqueue/dequeue counts must agree, input FSMs and pending masks must be idle, and live wake-button levels must be inactive. A failed check vetoes sleep and recovers normal owners/display without consuming the event. An IRQ arriving after the check remains pending and wakes WFI; it is serviced after clock, GPIO, and RTOS timebase restoration.

HW6 evidence `EV-HW6-20260813-P1-UICLOCK-046` validates the first UI-to-power requester link. Breakpoint-based target validation stopped after UI clock release for normal HOME dispatch and a helper-queued `NAV_MENU` event. The HOME transaction reported current page `HOME`, `event = 1`, and `ui clock req/rel = 3/3`; two earlier boot-gate UI checks accounted for the prior request/release pairs. The menu transaction reported `ui clock req/rel = 4/4`, `event = 3`, previous/current page `HOME/MENU`, reactive/release statuses `0x0/0x0`, UI requester status `UI=0x0`, selected/current profile `REACTIVE_BASE`, clear domains, `STOP2 ready = 1`, USB/VDDUSB/HSI48 off, and PLL2 off. This proves that `thUI` publishes and releases symbolic reactive clock intent for bounded shell/router work; it does not prove the measured reactive current, final animation/rendering policy, or physical-button path.

HW6 evidence `EV-HW6-20260813-P1-DISPLAYCLOCK-047` validates the first `thDisplay` display-transfer requester link for the boot clear-hold transfer. Breakpoint-command target validation stopped at the end of the display requester. The first hit showed `thDisplay` requesting `DISPLAY_TRANSFER_ACTIVE` (`D=0x8`), active capabilities `0x8`, and STOP2 blocked while display work was active. The second hit showed display clock request/release `1/1`, transfer/release statuses `0x0/0x0`, the display requester slot cleared, selected/current profile `REACTIVE_BASE`, and `STOP2 ready = 1` after release. This proves the OS plumbing for display-transfer clock intent and STOP2 blocker clearing around the boot clear-hold transaction; it does not yet prove settled HOME/UI render capture, partial-update policy, LPBAM, or final display scheduling.

HW6 evidence `EV-HW6-20260813-P1-AUDIOCLOCK-048` validates the no-sound audio clock scaffold: `thAudio` requests and releases `SAI_AUDIO_ACTIVE` through the same requester-specific `thPower` ACK path. Request-side evidence showed requester cap `A=0x4`, required/managed domains `0x4/0x4`, SAI kernel `4096000 Hz`, and STOP2 blocked while held. Release-pulse evidence then showed audio request/release `1/1`, release reason/caps/status `3/0x0/0x0`, requester cap `A=0x0`, domains `0x0/0x0`, SAI kernel `0 Hz`, and STOP2 ready again. This is no-sound clock plumbing only; it does not validate PCM playback, reactive SFX duration policy, realtime mixer behavior, current, underrun recovery, or production audio scheduling.

Other clock profiles remain scaffolded. Runtime and UI can now publish and release `REACTIVE_TRANSACTION_ACTIVE`; runtime can also publish, hold, suspend, resume, and return `REALTIME_DEADLINE_ACTIVE`; and display can publish and release `DISPLAY_TRANSFER_ACTIVE` for bounded transfer windows. The validated base `24 MHz` path and high-I/O path do not yet constitute the final workload ladder. `CLK_REACTIVE_BURST`, `CLK_REALTIME_BALANCED`, any intermediate PLL operating points, measured current, transition energy, hysteresis, and production PLL2/autonomous-domain policy still must be enabled one at a time after clock readbacks, TraceX timing, and current evidence pass. Packages and scene types never select those points directly.
---

## Reactive Transaction Policy

A reactive transaction begins when the runtime receives an admitted event. It ends only after:

1. bounded package/Engine logic has completed
2. resulting state transitions have settled
3. required rendering and owner requests have completed or reached a declared asynchronous boundary
4. the next event interests, schedules, and waiting visual are established
5. the runtime yields to PeepOS

After the yield, PeepOS selects the deepest compatible sleep state immediately. Waiting for input is not an active period and does not require an inactivity timeout.

A waiting visual may continue changing while the CPU sleeps. That visual motion is cosmetic presentation only; it does not advance committed package state. Gameplay state advances only in admitted Engine transactions.

---

## Interaction State Policy

PeepOS always owns an `ACTIVE`/`INACTIVE` interaction state that protects a keychain device from unintended interaction. It is independent of CPU awake/sleep state.

A package declares:

- meaningful activity sources that refresh the interaction window
- one inactive route: preserve the current scene, transition to a declared scene, or exit to shell
- inactive waiting-visual intent or fallback
- any statically bounded work that may defer inactivity until completion

Rules:

- the active target/system policy owns the inactivity timeout; a package cannot disable it or author a replacement timeout
- while `INACTIVE`, only target/system-admitted activation gestures restore normal package interaction
- HW6 initially admits Start; future measured policy may admit another button or a classified chord such as `L+R`
- other package input and sensor actions are suppressed while `INACTIVE` unless a system-owned safety policy explicitly requires otherwise
- the physical activation gesture is consumed by PeepOS and is not delivered as a package action
- the Engine receives symbolic `DEVICE_INACTIVE` after the declared inactive route settles and `DEVICE_ACTIVE` after focus and scene state are valid
- activation does not run a second package-selected route: preserved scene state resumes, a declared inactive target remains active, and an `exit_to_shell` route remains in shell
- inactivity deferral must have a statically bounded completion or timeout; unbounded deferral is invalid
- a bounded cinematic may defer inactivity until completion
- admitted gyro or other non-button control events may count as meaningful activity when declared by the active block and supported by the target profile
- passive animation, cosmetic LPBAM playback, keepalives, and arbitrary activity hints do not count as meaningful user activity

If inactivity occurs during `REALTIME`, PeepOS suspends/stops frame-paced execution and follows the scene's declared inactive route before establishing the inactive wait state.

Package-authored gameplay inactivity behavior is separate. A designer may schedule a normal bounded event such as `explore -> pet_idle`; that timer causes an ordinary state transition and does not configure PeepOS interaction state.

An admitted interactive communication context may receive only the bounded peer-wait treatment granted by the selected target profile. It cannot create unbounded inactivity deferral, keep realtime execution alive indefinitely, or turn communication into a wake source on HW6 while that capability is blocked.

---

## Cadence Policy

Cadence requests are requests only:

- `REALTIME` frame cadence is granted only while realtime activity is valid
- `REACTIVE` work is event/schedule driven and has no free-running package tick
- input-triggered reactive work is serviced promptly where the selected profile permits, then yields again
- display-only waiting motion may use an autonomous backend without running package logic
- logical periodic gameplay updates require admitted schedule events even when the display animates autonomously

Target profiles must define:

- interaction-state default/minimum/maximum inactivity timeout
- maximum bounded inactivity-deferral duration
- target-owned activation gestures and whether their physical input is consumed
- reactive input-response latency cap
- reactive scheduled-event cadence cap
- realtime frame budget and target frame rate
- waiting-visual animation availability, authored limits, and compiler-admission policy
- whether non-autonomous waiting visuals require MCU wake/update/return behavior
- supported meaningful-activity sources
- package-visible wake intents and lifecycle wake reasons

The previous 10-15 second inactivity value is only a provisional interaction-state UX target. It is not a delay before reactive sleep.

Profile-dependent waiting-visual behavior:

| Target Profile | Waiting Visual Backend |
|---|---|
| `HW6_VALIDATED_BASELINE` | hold the settled frame or use bounded wake/update/return behavior where admitted |
| `HW6_VALIDATED_LPBAM` | compile eligible waiting visuals into autonomous display sequences; fall back to hold or bounded wake/update/return when they do not fit |

---

## Transition Sequence (Required)

1. collect quiesce acknowledgements from owners
2. apply profile and sleep transition
3. validate HAL and RTOS timebases
4. resume owners and validate liveness
5. publish mode transition complete event

All waits in this sequence must have explicit timeout.

HW6 FW0 evidence `EV-HW6-20260811-P1-SLEEP-034` validates the pre-STOP portion of this sequence: `thPower` enters `PWR_SLEEP_PREP`, collects owner ACKs through bounded queues, intentionally skips real STOP entry, and recovers to `PWR_ACTIVE_LP`. HW6 FW0 evidence `EV-HW6-20260811-P1-STOP2-035` validates the next manual scaffold steps: after the same owner-ACK barrier, `thPower` enters real STOP2, wakes from a physical START press, restores clocks, and returns the power FSM to `PWR_ACTIVE_LP`; probe API `27` additionally validates a staged active-owner path where audio/input/display/sensor/comm are active, quiesced, STOP2-entered, then resumed or confirmed live after wake. Storage/flash were intentionally excluded from that active-owner proof to avoid rerunning flash scratch/erase/write tests. Evidence `EV-HW6-20260813-P1-STOP2ELIG-052` adds a production-shaped STOP2 eligibility dry-run ledger that reports current blockers without entering STOP2, evidence `EV-HW6-20260813-P1-STOP2CTRL-053` validates the default-off controlled entry helper that gates real STOP2 entry on a clear eligibility ledger, and evidence `EV-HW6-20260814-P1-STOP2WAKE-056` validates controlled physical START-wake classification with `DBGMCU->CR` debug-low-power bits cleared. Automatic idle admission began with a default-off scaffold, target-validated dry-run probe (`EV-HW6-20260814-P1-STOP2AUTO-055`), knob-enabled conservative LPBAM blocker proof (`EV-HW6-20260814-P1-STOP2AUTO-LPBAM-057`), and display-owned LPBAM readiness scaffold proof (`EV-HW6-20260814-P1-DISPLPBAM-058`): settled display state is no longer a false STOP2 blocker, but live auto entry can use the held-frame backend for baseline current work, while the LPBAM backend still refuses to sleep until `thDisplay` reports explicit LPBAM readiness. Exact second-entry counter capture, real LPBAM-ready automatic STOP2 entry, non-START wake-source classification, tick compensation, LPBAM waiting visuals, and fault-injection behavior remain open.
---

## Forbidden Patterns

- package code requesting direct STOP entry
- clock profile writes outside power owner
- package/runtime requests for literal clock frequencies or voltage scales
- unmeasured dynamic clock switching or switching during active DMA/bus work
- polling loops used instead of wake events
- entering installer mode with active non-USB storage users

---

## Required Measurements

For every candidate active operating point, record the full internal configuration used for evidence: SYSCLK/HCLK, voltage scale, flash latency/cache state, relevant kernel clocks, instrumentation state, and firmware/configuration identity.

Reactive sweeps must record:

- input/event to first visible response and settled presentation latency
- active transaction duration through return to the selected waiting backend
- charge/energy per complete event-to-yield transaction
- resulting STOP/wait residency and steady waiting current
- transaction, owner, DMA, wake, and resume failures

Realtime sweeps must record:

- sustained average and peak current plus charge/energy per frame where practical
- frame-time distribution, worst-case frame time, deadline misses, and available headroom
- audio underruns/glitches and required audio/kernel-clock stability
- sensor, owner, display, and storage deadline behavior under representative contention
- transition behavior into the declared inactive/end/failure route

Operating-point transition tests must record switch latency, switch charge/energy, failure behavior, and the measured break-even interval used for any hysteresis.

Track evidence and promoted selections in [[Power_Validation]], [[Pending_Measured_Constants_Register]], and [[HW6_Clock_Tree_Contract]].
