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

- runtime unit, runtime class, and `REACTIVE`/`REALTIME` execution intent
- reactive wait contract and admitted event interests
- waiting-visual intent and fallback associated with a settled reactive state
- realtime cadence/frame budget and declared meaningful work
- latency tolerance and symbolic wake intents
- temporary capability context declarations
- declared reactive fallback route
- optional automatic input-lock enablement, lock route, and meaningful-activity sources
- bounded lock deferral for declared non-interruptible work

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
- enabled input-lock state and admitted meaningful activity

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

`LP_GRAPH`
- reactive event/schedule/state transactions
- no awake wait loop
- Platform may hold or autonomously animate the settled display while waiting

`LP_MODULE`
- Engine-hosted reactive transactions with bounded awake work
- menus, dialogue, inventory, and similar modules yield between admitted events

`RT_SCENE`
- frame-paced active execution
- explicit meaningful-activity rules, suspend/resume behavior, and reactive fallback

`INSTALLER`
- USB/staging ownership mode with strict subsystem isolation
- reactive by default; it is a Platform-owned runtime class, not normal package gameplay

---

## Engine Execution Semantics

The Engine exposes execution semantics and game intent, not hardware power states.

| Semantic | Meaning |
|---|---|
| `REACTIVE` | Run a bounded event/state transaction, settle Engine actions and presentation, then yield until an admitted input, schedule, sensor, lifecycle, or system event occurs |
| `REALTIME` | Keep frame-paced game logic and rendering active while the realtime unit remains admitted |

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
5. Runtime/gameplay requests `REALTIME_DEADLINE_ACTIVE`: `thPower` selects the lowest measured realtime operating point with frame/audio/sensor/display margin.
6. Menu/input/static-display work requests `REACTIVE_TRANSACTION_ACTIVE`: `thPower` selects the lowest measured reactive point that meets response limits and returns to the selected waiting backend efficiently.

The first FW0 implementation is split between resolver scaffolding, one validated active USB path, one validated normal-boot cleanup path, a target-validated storage requester wrapper for MSC export/reclaim and explicit flash/staging provisioning, a target-validated runtime requester hook for bounded reactive transactions, a target-validated UI requester hook for bounded shell/router transactions, and a target-validated display-transfer requester hook for the boot clear-hold transfer. FW0 reports requested capabilities, selected internal profile, blockers, requester slots, per-requester status, domain readback, and current profile status. Owners request capabilities; `thPower` resolves the profile, storage remains the owner of USB device hardware and MSC media sequencing, and `thRuntime`/`thUI` publish symbolic execution intent only. Storage service boundaries use a named storage requester wrapper: MSC export/reclaim request `USB_DEVICE_ACTIVE | OCTOSPI_ACTIVE`, explicit flash/staging provisioning requests `OCTOSPI_ACTIVE`, and release clears the storage requester after cleanup. Runtime command boundaries use a named runtime requester wrapper: bounded reactive transactions request `REACTIVE_TRANSACTION_ACTIVE` and release that requester slot after the transaction returns. UI shell/router boundaries use a named UI requester wrapper: HOME dispatch, deferred router events, and input-driven UI actions request `REACTIVE_TRANSACTION_ACTIVE` and release after the bounded UI transaction returns. Display transfer boundaries use a named display requester wrapper: the validated FW0 boot clear-hold transfer requests `DISPLAY_TRANSFER_ACTIVE` and releases after the bounded display transaction completes; queued UI render calls are wrapped the same way but still need a settled HOME-render target capture.

HW6 evidence `EV-HW6-20260812-P1-CLOCKUSB-037` validates the active USB handoff: MSC export requests `USB_DEVICE_ACTIVE`, `thPower` applies the `CLK_IO_HIGH` path, the host mounts the staging FAT volume, and reclaim closes FileX/LevelX, releases the storage requester slot, returns to the base `24 MHz` profile, and disables USB clock/VDDUSB/HSI48.

HW6 evidence `EV-HW6-20260812-P1-CLOCKBOOT-038` validates the normal-boot cleanup path: after CubeMX/generated boot leaves USB PCD/VDDUSB state present, `thPower` sends a short storage-owned USB boot-park command. `thStorage` only parks USB device hardware, refreshes clock readback, and ACKs; it does not run full storage/flash initialization, FileX/LevelX, or MSC export. The validated boot result has HOME rendered, USB clock/VDDUSB/HSI48 disabled, and no long storage OSPI action.

HW6 evidence `EV-HW6-20260812-P1-CLOCKSTORAGE-039` validates the named storage requester wrapper for MSC export/reclaim. During active MSC, the storage requester slot held `USB_DEVICE_ACTIVE | OCTOSPI_ACTIVE` (`ST=0x3`), the selected/current profile was `CLK_IO_HIGH`, required/managed/readback domains were `0x3/0x3/0x3`, and STOP2 was blocked. After safe eject/detach and reclaim, the storage requester released to `ST=0x0`, selected/current returned to `REACTIVE_BASE`, required/managed/readback domains were `0x0/0x0/0x0`, STOP2 was ready, USB clock/VDDUSB/HSI48 were off, and PLL2 was off.

HW6 evidence `EV-HW6-20260812-P1-FLASHINIT-040` validates explicit flash/staging provisioning through the same storage requester wrapper. The destructive flash init command completed with flash init status `0x0`, wake/layout/FileX-LevelX/deep-power-down statuses `0x0/0x0/0x0/0x0`, erased `1280` blocks in the USB staging/export region, formatted/opened/flushed/closed FileX with all statuses `0x0`, and mounted as an empty host volume afterwards. Clock policy returned to `REACTIVE_BASE`, storage clock flash/release statuses were `0x0/0x0`, STOP2 was ready, domains were clear, USB/VDDUSB/HSI48 were off, and PLL2 was off after release.

HW6 evidence `EV-HW6-20260813-P1-RUNTIME-044` validates the first runtime-class scaffold against power-policy terminology. Normal shell boot reports `SHELL / REACTIVE / RUNNING`; package transfer enters `INSTALLER / REACTIVE / RUNNING`; the valid-package prompt remains in `INSTALLER`; and install-stub completion returns to `SHELL`. This evidence validates naming and lifecycle plumbing only. It does not validate measured reactive current, realtime admission, package execution, or final installer commit behavior.

HW6 evidence `EV-HW6-20260813-P1-RUNTIMECLOCK-045` validates the first runtime-to-power requester link. After boot, `thRuntime` reported one reactive clock request and one release (`runtime clock req/rel = 1/1`), reactive and release statuses were `0x0`, the runtime requester slot `RT` settled back to `0x0`, per-requester status reported `RT=0x0`, `STOP2 ready` remained `1`, and USB/VDDUSB/HSI48 plus PLL2 were off in idle. This proves the OS plumbing for runtime clock intent and requester-specific ACK/status handling; it does not prove the measured reactive operating point, realtime admission, package execution, or final scheduling policy.

HW6 evidence `EV-HW6-20260813-P1-UICLOCK-046` validates the first UI-to-power requester link. Breakpoint-based target validation stopped after UI clock release for normal HOME dispatch and a helper-queued `NAV_MENU` event. The HOME transaction reported current page `HOME`, `event = 1`, and `ui clock req/rel = 3/3`; two earlier boot-gate UI checks accounted for the prior request/release pairs. The menu transaction reported `ui clock req/rel = 4/4`, `event = 3`, previous/current page `HOME/MENU`, reactive/release statuses `0x0/0x0`, UI requester status `UI=0x0`, selected/current profile `REACTIVE_BASE`, clear domains, `STOP2 ready = 1`, USB/VDDUSB/HSI48 off, and PLL2 off. This proves that `thUI` publishes and releases symbolic reactive clock intent for bounded shell/router work; it does not prove the measured reactive current, final animation/rendering policy, or physical-button path.

HW6 evidence `EV-HW6-20260813-P1-DISPLAYCLOCK-047` validates the first `thDisplay` display-transfer requester link for the boot clear-hold transfer. Breakpoint-command target validation stopped at the end of the display requester. The first hit showed `thDisplay` requesting `DISPLAY_TRANSFER_ACTIVE` (`D=0x8`), active capabilities `0x8`, and STOP2 blocked while display work was active. The second hit showed display clock request/release `1/1`, transfer/release statuses `0x0/0x0`, the display requester slot cleared, selected/current profile `REACTIVE_BASE`, and `STOP2 ready = 1` after release. This proves the OS plumbing for display-transfer clock intent and STOP2 blocker clearing around the boot clear-hold transaction; it does not yet prove settled HOME/UI render capture, partial-update policy, LPBAM, or final display scheduling.

Other clock profiles remain scaffolded. Runtime and UI can now publish and release `REACTIVE_TRANSACTION_ACTIVE`, and display can publish and release `DISPLAY_TRANSFER_ACTIVE` for bounded transfer windows, but `CLK_REACTIVE_BURST`, `CLK_REALTIME_BALANCED`, PLL2 autogating interactions, current, transition energy, and hysteresis must still be enabled one at a time after clock readbacks, TraceX timing, and current evidence pass. FW0 currently leaves PLL2 readback active because autogate is intentionally disabled until SAI/OCTOSPI owner interactions and LPBAM cases are validated.
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

## Input Lock Policy

Automatic input locking protects a keychain device from unintended interaction. It is optional package policy implemented by PeepOS.

A package declares whether automatic locking is enabled. When enabled, it also declares:

- meaningful activity sources that reset the lock timer
- one lock route: preserve current state, transition to a declared package state, or exit to shell
- locked waiting-visual intent or fallback
- any statically bounded work that may defer locking until completion

Rules:

- the active target/system policy owns the lock timeout; a package may disable automatic locking but does not author a replacement timeout
- while locked, only `START` is armed to wake and unlock normal package interaction
- A/B/L/R, joystick, and any other profile-granted package input or sensor actions are suppressed while locked unless a future system-owned safety policy explicitly requires otherwise
- the physical `START` press used to unlock is consumed by PeepOS and is not delivered as a package action
- the Engine receives symbolic `DEVICE_LOCKED` after the declared lock route settles and `DEVICE_UNLOCKED` after focus and runtime state are valid
- unlock does not run a second package-selected route: preserved state resumes, a declared lock target remains the active package state, and an `exit_to_shell` route remains in shell
- a package may disable automatic locking
- lock deferral must have a statically bounded completion or timeout; unbounded deferral is invalid
- a bounded cinematic may defer locking until completion
- admitted gyro or other non-button control events may count as meaningful activity when declared by the active block and supported by the target profile
- passive animation, cosmetic LPBAM playback, keepalives, and arbitrary activity hints do not count as meaningful user activity

If locking occurs during `REALTIME`, PeepOS suspends/stops frame-paced execution and follows the unit's declared reactive fallback before establishing the locked wait state.

Package-authored gameplay inactivity behavior is separate. A designer may schedule a normal bounded event such as `explore -> pet_idle`; that timer causes an ordinary state transition and does not configure the PeepOS input lock.

An admitted interactive communication context may receive only the bounded peer-wait treatment granted by the selected target profile. It cannot create unbounded lock deferral, keep realtime execution alive indefinitely, or turn communication into a wake source on HW6 while that capability is blocked.

---

## Cadence Policy

Cadence requests are requests only:

- `REALTIME` frame cadence is granted only while realtime activity is valid
- `REACTIVE` work is event/schedule driven and has no free-running package tick
- input-triggered reactive work is serviced promptly where the selected profile permits, then yields again
- display-only waiting motion may use an autonomous backend without running package logic
- logical periodic gameplay updates require admitted schedule events even when the display animates autonomously

Target profiles must define:

- automatic input-lock default/minimum/maximum timeout when a package enables it
- maximum bounded lock-deferral duration
- the system unlock action and whether its physical press is consumed
- reactive input-response latency cap
- reactive scheduled-event cadence cap
- realtime frame budget and target frame rate
- waiting-visual animation availability, authored limits, and compiler-admission policy
- whether non-autonomous waiting visuals require MCU wake/update/return behavior
- supported meaningful-activity sources
- package-visible wake intents and lifecycle wake reasons

The previous 10-15 second inactivity value is only a provisional UX target for packages that enable automatic locking. It is not a delay before reactive sleep.

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

HW6 FW0 evidence `EV-HW6-20260811-P1-SLEEP-034` validates the pre-STOP portion of this sequence: `thPower` enters `PWR_SLEEP_PREP`, collects owner ACKs through bounded queues, intentionally skips real STOP entry, and recovers to `PWR_ACTIVE_LP`. HW6 FW0 evidence `EV-HW6-20260811-P1-STOP2-035` validates the next manual scaffold steps: after the same owner-ACK barrier, `thPower` enters real STOP2, wakes from a physical START press, restores clocks, and returns the power FSM to `PWR_ACTIVE_LP`; probe API `27` additionally validates a staged active-owner path where audio/input/display/sensor/comm are active, quiesced, STOP2-entered, then resumed or confirmed live after wake. Storage/flash were intentionally excluded from that active-owner proof to avoid rerunning flash scratch/erase/write tests. These evidence items do not yet validate production automatic STOP admission, wake-source classification policy, tick compensation, LPBAM waiting visuals, current, repeated sleep/wake cycles, or fault-injection behavior.

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
- transition behavior into the declared reactive fallback

Operating-point transition tests must record switch latency, switch charge/energy, failure behavior, and the measured break-even interval used for any hysteresis.

Track evidence and promoted selections in [[Power_Validation]], [[Pending_Measured_Constants_Register]], and [[HW6_Clock_Tree_Contract]].
