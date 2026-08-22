# RTOS Ownership and Queue Topology

This document defines thread responsibilities, queue/event topology, and ISR signaling rules.

---

## Thread Ownership Model

Recommended baseline owners:

| Thread | Owns |
|---|---|
| `thPower` | mode state, sleep class, clock transitions |
| `thDisplay` | display bus and display transfer path; publishes bounded display-transfer clock intent through `thPower` |
| `thAudio` | audio bus, audio DMA, amp control; publishes bounded SAI/audio clock intent through `thPower` |
| `thInput` | raw input capture and logical action routing |
| `thUI` | shell and shared UX service flow; publishes bounded shell/router reactive clock intent through `thPower` |
| `thRuntime` | runtime host manager dispatch; tracks system host, active package scene type/ID, bounded scene state/actions, execution semantic, lifecycle, shell/installer return context, waiting-presentation publication, and symbolic runtime clock intent requests through `thPower`; it does not own display hardware |
| `thStorage` | flash, filesystem, install pipeline |
| `thSensor` | sensor bus, sensor policy, health publication |
| `thComm` | BLE/NINA module, communication UART, communication policy |

---

## Queue Topology Baseline

Required queues:
- `qInputRaw` ISR -> `thInput`
- `qUIEvents` `thInput` -> `thUI`
- `qRuntimeEvents` `thInput`/`thUI` -> `thRuntime`
- `qSysEvents` multi-producer -> `thPower`
- `qDisplayCmd` multi-producer -> `thDisplay`
- `qAudioCmd` multi-producer -> `thAudio`
- `qStorageReq` multi-producer -> `thStorage`
- `qSensorReq` multi-producer -> `thSensor`
- `qCommCmd` multi-producer -> `thComm`

Exact message shapes are defined in [[Interface_Control_Document]].

In HW6 FW0, `qInputRaw` carries one four-word A/B/L/R edge record per EXTI
transition. The ISR uses a non-blocking send and returns; `thInput` consumes the
records in FIFO order, applies ThreadX-tick debounce deadlines, and alone emits
logical events. A send failure is counted and leaves the raw-level recovery mask
available to `thInput`. Queue high-water may remain zero when ThreadX delivers a
message directly to an already-waiting receiver; enqueue/dequeue/drop counts are
the authoritative loss check.

---

## Event Flag Groups

Recommended groups:
- `egMode` system host, package scene, execution semantic, and lifecycle state
- `egPower` quiesce/resume coordination
- `egHealth` subsystem health/fault bits
- `egDebug` debug mode toggles

Bit ownership must be explicit and centrally defined.

---

## ISR Signaling Discipline

ISRs may:
- enqueue small raw events
- set thread flags

ISRs may not:
- perform long HAL calls
- touch filesystem/storage APIs
- change clocks/sleep states
- block or busy-wait

---

## Object Creation and Determinism

- All RTOS objects are created during init phase only.
- No runtime object creation after scheduler start.
- Thread stack sizes and queue depths are compile-time defined.
- All cross-thread waits must be bounded with explicit timeout.

---

## Forbidden Patterns

- multiple threads touching same peripheral handle
- queue payloads with transient pointers
- hidden mode changes from non-power owners
- polling loops where event-driven signaling exists

---

## HW6 Measured Scaffold Baseline

`EV-HW6-20260731-P5-RTOS-001` measured the first HW6 implementation of this
topology on `HW6-UNIT-001`:

- all nine owner threads started (`0x1FF / 0x1FF`)
- all nine queues accepted and delivered a fixed startup envelope
  (`0x1FF / 0x1FF`)
- all four event groups passed create/set/get checks (`0x0F / 0x0F`)
- every owner used a bounded wait and reported zero message errors
- the ThreadX low-power scheduler path reached normal CPU `WFI`

The diagnostic implementation allocated one 1 KiB stack and one eight-entry,
four-`ULONG` queue per owner from the generated 16 KiB ThreadX byte pool. Pool
availability changed from 16,376 to 5,864 bytes, a measured scaffold cost of
10,512 bytes including allocator overhead.

These figures prove that the topology fits and schedules on HW6. They are not a
final memory budget: stack high-water marks, production message sizes, queue
depths, and runtime burst behavior have not yet been measured. The startup
envelope is diagnostic only; production message contracts remain under
[[Interface_Control_Document]] authority.

Current FW0 stack sizing is compile-time tunable through `KNOB_RTOS_DEFAULT_STACK_BYTES`, `KNOB_RTOS_POWER_STACK_BYTES`, `KNOB_RTOS_INPUT_STACK_BYTES`, and `KNOB_RTOS_STORAGE_STACK_BYTES`. `thPower` deliberately has a larger stack budget than the default owner stack because it owns PMIC policy plus HAL clock-policy transitions during USB MSC, STOP2, and shutdown paths. `thInput` has a measured provisional `1536` byte stack budget because bounded TMAG3001 raw XYZ diagnostic capture overflowed the original `1024` byte default stack; a `4096` byte input diagnostic stack exhausted the current ThreadX byte pool and is not the accepted default. The RTOS probe records configured stack bytes plus ThreadX stack start/end/current/high-water pointers for each owner so stack damage can be separated from normal storage, USB reclaim, or input-owner diagnostic failures.

Phase 5 remains open for real producer/consumer routing, sole-owner peripheral
access, saturation and timeout policy, fault propagation, and the power
quiesce/resume barrier.

## HW6 FW0 Normal Boot Lifecycle Baseline

`EV-HW6-20260810-P5-BOOT-021` records the prior FW0 normal boot behavior on
`HW6-UNIT-001` after the UI router and PMIC shipping-mode enable work:

- all nine owner threads, queues, and event groups initialize successfully
- `thDisplay` owns the first panel action after the display owner starts; the
  current FW0 target is an immediate clear/static hold to remove panel static
  before normal UI routing
- `thPower` performs the normal boot power stabilization path after the display
  boot hold has been requested
- before completing normal power boot, `thPower` may send a short storage-owned
  USB boot-park command; this parks generated USB PCD/clock/VDDUSB/HSI48 state
  without running full storage/flash initialization or MSC export
- the power stabilization path enables ADP5360 `EN_MR_SD` through the PMIC
  driver and then takes the read-only ADP5360 snapshot
- `thUI` dispatches HOME only after the power boot flag is complete

The measured RTOS probe values for the original v3 boot slice were `init/runtime complete = 1 / 1`, `boot power/display = 1 / 1`, owner started mask `0x1ff`, queue self-test mask `0x1ff`, event self-test mask `0xf`, and init status `0x0`. FW0 evidence `EV-HW6-20260812-P1-CLOCKBOOT-038` adds the storage-owned USB boot-park command: storage queue send/wait succeeded (`0/0`), the ACK flag was set, HOME rendered, USB clock/VDDUSB/HSI48 were off, and no long storage action ran during normal boot. HW6 evidence `EV-HW6-20260813-P1-RUNTIME-044` validates the first `thRuntime` scaffold: normal boot reports `SHELL / REACTIVE / RUNNING` (`class/exec/lifecycle = 1/1/2`), package MSC entry reports `INSTALLER / REACTIVE / RUNNING` (`1/5/1` class prev/current/return), a valid package prompt remains in `INSTALLER`, and install-stub completion returns to `SHELL` with no runtime error (`installer enter/done/err = 2/2/0`). HW6 evidence `EV-HW6-20260813-P1-RUNTIMECLOCK-045` validates the first `thRuntime` clock-intent requester hook: runtime commands request `REACTIVE_TRANSACTION_ACTIVE` through the power queue, `thPower` resolves and ACKs with requester-specific clock flags/status, runtime releases its requester slot after the command, and the validated idle capture returned requester cap `RT=0x0` with `STOP2 ready=1`. HW6 evidence `EV-HW6-20260813-P1-RUNTIMEADMIT-050` extends this to package-admission scaffolding: reactive package stubs enter `LP_MODULE / REACTIVE / RUNNING`, request `REACTIVE_TRANSACTION_ACTIVE`, release immediately, and leave `STOP2 ready=1`; realtime package stubs enter `RT_SCENE / REALTIME / RUNNING`, hold `REALTIME_DEADLINE_ACTIVE`, block STOP2 while admitted, and clear the requester on runtime return. HW6 evidence `EV-HW6-20260813-P1-RUNTIMESUSPEND-051` validates the runtime suspend/resume extension: realtime suspend saves class/execution/lifecycle/capability state, releases `REALTIME_DEADLINE_ACTIVE`, and makes STOP2 eligible; realtime resume re-requests the saved realtime capability and blocks STOP2 again until runtime return clears it. HW6 evidence `EV-HW6-20260813-P1-STOP2ELIG-052` validates the first `thPower` STOP2 eligibility dry-run: baseline shell is currently eligible, realtime runtime admission is blocked by the runtime requester clock capability, and runtime suspend clears that blocker again; the dry-run records pending owner quiesce and LPBAM-validation work but does not enter STOP2. HW6 evidence `EV-HW6-20260813-P1-UICLOCK-046` validates the matching `thUI` clock-intent requester hook: HOME dispatch and deferred `NAV_MENU` both request `REACTIVE_TRANSACTION_ACTIVE`, `thPower` ACKs through the UI requester slot, `thUI` releases after the bounded transaction, and the validated captures returned requester cap `UI=0x0` with `STOP2 ready=1`. HW6 evidence `EV-HW6-20260813-P1-DISPLAYCLOCK-047` validates the first `thDisplay` display-transfer clock-intent requester hook: the boot clear-hold transaction requests `DISPLAY_TRANSFER_ACTIVE`, `thPower` blocks STOP2 while requester cap `D=0x8` is active, `thDisplay` releases after completion, and the requester slot clears with `STOP2 ready=1`.

FW0 probe version 21 adds the first `thInput -> thRuntime` generic button path. `thInput` still does not define A/B/L/R meaning; it only chooses whether the generic press belongs to shell/installer/system overlays (`thUI`) or package runtime classes (`thRuntime`). The runtime side is currently a stub that records the event for later package-host integration. HW6 validation showed shell presses stay on `thUI` (`ui/runtime/overlay = 8 / 0 / 0`) and a reactive `LP_MODULE` stub press routes to `thRuntime` (`target/reason/status = 2 / 8 / 0x0`, runtime input count/button `1 / 1`) with zero runtime queue errors.

FW0 probe version 22 adds the first system-action admission scaffold. `thUI` is still only a caller for shell actions, and `thStorage` still owns MSC/export/install-stub work. Before a UI action enters MSC or package-install overlay work, the admission point records the action, denies duplicate overlay-entry attempts while a system overlay is already active, and requests `thRuntime` suspend with a bounded owner ACK when a package runtime class is running. The runtime ACK is set only after `thRuntime` handles the command, so admission is based on real owner processing rather than only on queue-send success. A dry-run GDB helper (`__fw0_admission_msc_enter_dry_run.gdb`) exercises this policy without starting USB MSC. HW6 evidence `EV-HW6-20260814-P1-ADMISSION-053` validates the dry-run path: reactive package stub entered `LP_MODULE / REACTIVE / RUNNING`; MSC-enter admission reported action/result/reason/status `1 / 2 / 3 / 0x0`, counts `request/allow/deny/suspend = 1 / 1 / 0 / 1`, and `thRuntime` moved to `SUSPENDED` with suspend count `1` and zero runtime queue errors.

FW0 probe version 23 extends system-action admission to power-owned shutdown preparation. `thPower` now asks the admission layer before START-shutdown, battery-critical, or boot-low-battery shipment prep starts physical owner quiesce. Power shutdown actions may preempt normal system overlays, but runtime resume is intentionally narrow: only START-cancel resumes a runtime that was suspended by START-shutdown admission; battery-critical and boot-low-battery shipment prep do not auto-resume package runtime. HW6 evidence `EV-HW6-20260814-P1-POWERADMIT-054` validates the dry-run path: a reactive package stub entered `LP_MODULE / REACTIVE / RUNNING`, START-shutdown admission reported action/result/reason/status `4 / 2 / 3 / 0x0`, `thRuntime` moved to `SUSPENDED` with suspend count `1`, and a START-cancel dry-run then reported resume reason/status `1 / 0x0` with `thRuntime` back at lifecycle `RUNNING` and suspend/resume counts `1 / 1`.

FW0 probe version 24 adds the automatic STOP2 idle admission path. It was introduced default-off for dry-run evidence and is now compiled enabled for the held-frame baseline; no new thread or queue is added: `thPower` owns the periodic check, reuses the existing STOP2 eligibility ledger, and calls the controlled-entry path only after runtime, UI/router, display-stable state, storage/USB, input, owner-queue, clock, PMIC, battery, and required-idle-window checks are clear. Display-stable state is based on display-owner completion/success and current-page match, not raw request/render counter equality. The LPBAM backend extends this with an explicit `thPower -> thDisplay` prepare/abort handoff and a separate display-owned readiness state. The GDB dry-run helper `__fw0_stop2_auto_idle_dry_run.gdb` records the same decision without entering STOP2. HW6 evidence `EV-HW6-20260814-P1-STOP2AUTO-055` validates the default-off dry-run after a settled menu render: automatic entry stayed disabled, no STOP2 attempt occurred, the display false-blocker cleared, and the remaining pending bits were expected for owner quiesce, LPBAM validation, and the idle window. HW6 evidence `EV-HW6-20260816-P1-STOP2AUTO-PERIODIC-067` validates that the compiled default-on held-frame policy can enter real STOP2 from the periodic `thPower` check, wake on START, and return with clean owner quiesce/recovery status; exact second-entry counters remain to be captured separately.
FW0 probe version 36 adds a boot-idle peripheral park ledger. Once the shell/runtime and boot power cleanup are complete, `thPower` sends bounded mode commands to the communication and sensor owners so NINA and LIS settle into their default STOP2-resident modes before the first automatic or manual STOP2 request. STOP2 eligibility then checks that idle-park readiness directly and reports `IDLE_PARK` (`0x4000`) if the modules are not already parked. This keeps normal idle close to STOP2 and avoids hiding multi-second first-time BLE/LIS setup inside a STOP2 entry request.
HW6 evidence `EV-HW6-20260816-P1-STOP2IDLEPARK-068` validates that ledger on target: boot-idle park completed once with `status=0x0`, communication and sensor owner ACKs arrived, automatic STOP2 reached two entries with no hard blockers, `stop2_eligibility_idle_peripheral_park_ready=1`, and the second owner quiesce interval was zero ticks. The one-time boot park still consumed the expected BLE/LIS setup time, so later work should focus on moving any remaining mandatory setup earlier or making it asynchronous without weakening owner boundaries.
FW0 probe version 37 and owner probe version 17 add the first real display LPBAM prepare/start/abort path. No new thread is added: `thPower` selects the LPBAM display backend only under test/profile policy, sends a bounded prepare command to `thDisplay`, waits for the display-owner ACK/status, rechecks the STOP2 ledger, and sends a bounded abort command if a late blocker appears or after wake. `thDisplay` owns SPI3, LPDMA1, LPTIM1, SRAM4 payloads/descriptors, and normal display-owner reclaim. The current validated payload is a renderer-cursor bring-up slice: `display_renderer.c` exposes the selected cursor's native panel region, the LPBAM buffer compiler builds four cursor-blink frame states and dirty-row chunks, and the LPBAM scenario links a circular LPDMA queue with one LPTIM1-triggered payload per animation frame. HW6 evidence `EV-HW6-20260816-P1-LPBAMCURSOR-070` reports backend `2/2/0x0/1`, cursor region `153/8/73/16`, payload `4/4/732`, queue nodes `24`, all fill/clock/link/start and abort statuses `0x0`, owner STOP2 count `2`, and visible autonomous animation. Visual correctness remains open because the cursor rows did not blink in unison; the next renderer step is dirty-row tracking so normal partial display updates and LPBAM payload compilation share one source of truth.
FW0 probe version 38 and owner probe version 20 add the awake cursor blink and held-frame STOP2 display handoff. No new thread is added: `thDisplay` owns the cursor-visible render; `thPower` suppresses the periodic blink scheduler, directly applies/releases display-transfer clock capability because it owns the clock-policy resolver, sends a bounded cursor-visible command to `thDisplay`, waits for owner ACK/status, and proceeds with held-frame STOP2 only if the ledger is still clear. This keeps awake diagnostic animation out of the STOP2 entry window without moving panel operations into `thPower`. HW6 evidence `EV-HW6-20260817-P1-AWAKEBLINK-072` reports awake HOME blink visually confirmed, blink request/render `14/14`, dirty rows `8/153/160`, handoff count/status/send/wait/ack/owner `1/0x0/0x0/0x0/0x108/0x0`, no hard blockers, and owner STOP2 `1/594/594/594`.
FW0 probe version 42 and owner probe version 24 close the first complete-cursor LPBAM transfer framework without changing ownership. `thDisplay` still owns renderer intent, SRAM4 payload/descriptor compilation, SPI3, LPDMA1, and LPTIM1; `thPower` still owns admission, STOP2 entry, and the bounded prepare/abort lifecycle. The display compiler splits each `183`-byte cursor payload into a `180`-byte 32-bit DMA body and `3`-byte byte tail, and SPI3 uses a four-frame FIFO threshold so the word-wide body drains correctly in STOP2. HW6 evidence `EV-HW6-20260818-P1-LPBAMCURSOR32-073` reports payload `4/4/732`, queue nodes `28`, FIFO threshold `4`, all prepare/start/abort statuses `0x0`, owner STOP2 count `2`, visually unified cursor blinking, and successful repeated wake/re-entry.
FW0 probe version 48 keeps that ownership unchanged while making the implementation survive CubeMX regeneration. `ps_lpbam_display_queue.c` now owns the runtime queue object, SRAM4 descriptors, panel payload nodes, and circular-list construction; `ps_lpbam_dma_node_compat.c` owns the three LPBAM node-helper replacements that explicitly restore `DMA_NORMAL`. CubeMX scenario build/config and overwrite-prone basic common/SPI helpers remain generated scaffolding but are excluded from the target. HW6 evidence `EV-HW6-20260820-P1-LPBAMREGEN-074` reports prepare/commit/abort counts `5/5/5`, queue nodes `28`, all queue/start/reclaim statuses `0x0`, owner STOP2 count `5`, and unchanged visible behavior after regeneration and a clean build.
The same evidence closes the earlier `25`-tick owner scheduling gap: LPBAM prepared at tick `1373`, committed at `1374`, and reached WFI at `1374`. `thPower` now receives a direct recheck after the display clock is released and preserves accumulated idle time only for the cursor-blink transfer participating in the handoff.

FW0 probe version 49 and button API version 10 add ordered A/B/L/R edge delivery and a final STOP2 input veto without changing ownership. Immediately before real WFI, after LPBAM commit and owner quiesce, `thPower` masks interrupts and atomically checks all owner queues, raw input enqueue/dequeue counts, input FSM state, and live button GPIO levels. If that check fails, `thPower` refuses STOP2 and recovers the owners/display so `thInput` can consume the event. If an edge arrives after the check, its IRQ remains pending and wakes WFI for normal processing after restore. HW6 evidence `EV-HW6-20260820-P1-REACTIVELPBAM-075` reported raw ISR send/process/drop `32/32/0`, queue enqueue/dequeue/drop `32/32/0`, logical press/policy delivery/UI action `16/16/16`, final check/veto/status `3/0/0x0`, and three successful owner STOP2 entries. The user also found the awake-to-LPBAM cursor transition visually imperceptible. This closes the tested ordered-input and cursor-handoff slice; long/repeat/chord/stuck input and general authored animation remain open.

FW0 RTOS probe version 50 and owner probe version 25 add the bounded waiting-animation boundary while preserving thread ownership. `thDisplay` obtains a read-only renderer-owned program descriptor, authors each phase framebuffer, and passes only prepared frames plus candidate native rows to the generic LPBAM packer. The descriptor is static renderer storage rather than a display-thread local, reducing the prepare call's compiled stack frame from `96` to `40` bytes after a target `STKOF` capture exposed the default `1024`-byte `thDisplay` stack limit. `thPower` remains the sole STOP2 admission owner. If temporary display/UI work invalidates a pending handoff, `thDisplay` records an explicit one-bit rearm need and requests an immediate `thPower` recheck only after display eligibility returns; `thPower` then reevaluates idle, input, queues, clocks, runtime, UI, and all other blockers before issuing a fresh handoff. Evidence `EV-HW6-20260820-P1-WAITANIM-077` reported request/run `7/7`, prepare count `7`, commit and owner STOP2 counts `6/6`, all prepare/start/abort statuses `0x0`, and responsive repeated input across varied timings. The successful run had rearm pending/count `0/0`, so the no-loss normal path is validated while direct execution of the cancellation-rearm branch remains open for controlled fault-injection evidence.

Owner probe display-driver API version `2` separates the two SPI3 TX DMA roles without changing the sole-owner boundary. Awake `thDisplay` transfers pack into normal SRAM and use GPDMA1 channel 0; STOP2 LPBAM queue construction and execution continue to address LPDMA1 channel 0 explicitly with SRAM4-resident payloads/descriptors. Because CubeMX MSP initialization links LPDMA last, both boot initialization and every post-LPBAM SPI restore explicitly relink `hspi3.hdmatx` to GPDMA after `HAL_SPI_Init()`. Evidence `EV-HW6-20260820-P1-DMASRAMSPLIT-078` reports post-wake SPI/GPDMA handle equality, a distinct LPDMA handle, awake DMA error `0x0`, display completion/success/status `1/1/0x0`, normal-SRAM `txBuf`, SRAM4 LPBAM arena/queue placement, and build SRAM4 use `12632/16384` bytes.

Evidence `EV-HW6-20260821-P1-LPBAMPOOL-079` preserves the same ownership while replacing the fixed four-by-three chunk partition with one deterministic `12`-chunk compiler pool. `thDisplay` remains the only thread that compiles sequence entries and builds the LPDMA queue. Admission reports sequence, chunk, and payload use before clocks are enabled or the queue is linked; an over-budget program returns an explicit argument, sequence, chunk, payload, or build reason instead of partially arming LPBAM. The validated cursor used `4/4` sequence entries, `4/12` chunks, and `575/9216` SRAM4 payload bytes with status/reason `0x0/0`, while repeated STOP2 behavior remained unchanged.

FW0 RTOS probe version `53` and owner probe version `28` add phase-preserving reverse handoff without adding a thread, queue, or cross-owner peripheral access. `thPower` still owns STOP2 completion and sends a bounded wake-abort command; `thDisplay` alone snapshots the circular LPDMA position and LPTIM counter, reclaims LPBAM hardware, restores SPI3/GPDMA, and presents the matching renderer sequence frame. `thPower` then resumes the awake blink scheduler at the remaining fraction of the same cadence interval. Evidence `EV-HW6-20260821-P1-LPBAMRESUME-083` reported a valid waiting-state snapshot at sequence/frame/phase `0/1/1`, LPTIM `6589/7814`, four ticks remaining, all snapshot/render/abort/resume statuses `0x0`, three completed STOP2 cycles, raw input enqueue/dequeue/drop `46/46/0`, and logical/policy counts `23/23`. Ordinary pre-entry LPBAM cancellation retains the existing reset behavior; only the post-WFI command requests timeline preservation. The active-transfer snapshot branch remains bounded and implemented but lacks separate target evidence.

The same RTOS/owner API boundary now supports the permanent six-band SRAM4 payload layout and single-flight automatic handoff. `thDisplay` remains the sole compiler and LPBAM hardware owner; it maps visual changes to fixed `28`-row bands and admits at most `18` payload transactions. `thPower` checks automatic admission every `100 ms` with no configured quiet delay, but does not issue another prepare while an LPBAM edge request is pending or in the requested state. The armed edge-time recheck remains allowed to complete the existing request. Evidence `EV-HW6-20260821-P1-LPBAMLAYOUT-084` validated a three-state full-panel program at `3/12` sequence entries, `18/18` chunks, and `10356/10512` wire/arena bytes. A subsequent mixed-timing input stress run reported raw queue enqueue/dequeue/drop `262/262/0`, logical press/policy/UI delivery `131/131/131`, final successful admission snapshots `206/206` with an empty queue, and `15` successful STOP2 entries. No thread, queue, dynamic allocation, or peripheral owner was added.

RTOS probe version `54` and owner probe version `30` generalize the waiting-animation handoff from one cursor bit to an exact combined sequence frame. `thDisplay` still owns composition, awake GPDMA presentation, LPBAM compilation, LPDMA progress mapping, and preferred-timeline restoration; `thPower` still owns STOP2 admission and wake completion. The scheduler records the next representable `1 -> 0` handoff frame before compile-ahead, renders that exact frame at commit, retains the live LPBAM frame and remaining cadence interval on wake, and resumes the preferred descriptor from the matching visual state. Evidence `EV-HW6-20260822-P1-LPBAMPHASE-085` validated a six-step two-phase/three-phase program at `6/12` sequence entries, `12/18` transactions, and `2920/10512` payload bytes. Wake selected/preferred frame was `0/0`, resumed frame/count was `0/6`, mapping and resume statuses were `0x0`, eight STOP2 entries completed, and the user confirmed stable visible `1,2,3` playback after repeated stops. No owner, queue, thread, or dynamic allocation was added.

The compiler fallback remains one bounded `thDisplay` operation: attempt the preferred descriptor once, then on resource rejection attempt one generated three-step descriptor, then report held-frame fallback if that also fails. No new queue round trip, dynamic allocation, element-priority search, or repeated admission loop is permitted. `thPower` consumes only the final owner result and remains responsible for backend selection and STOP2 entry.

Owner probe version `31` adds an immutable snapshot of the descriptor that actually compiled so a later wake restoration of the preferred descriptor cannot erase fallback evidence. Evidence `EV-HW6-20260822-P1-LPBAMGUARANTEED-086` used a four-state full-panel preferred scene plus the cursor: preferred compilation rejected at `18/18` transactions and `10512/10512` payload bytes with `CHUNKS`, the one guaranteed retry compiled `3/12` global steps with phases `0/1/2`, and STOP2 visibly played all three coherent states. Wake mapped the guaranteed frame into the four-state preferred descriptor with successful map/resume status, and a temporary target-only probe confirmed physical awake presentation of omitted preferred phase `3`. That probe and its scheduling override were removed before the cleaned API `54/31` baseline build and target run. Thread, queue, peripheral ownership, and dynamic-allocation policy are unchanged.

FW0 RTOS probe version `56` and scene-runtime API version `3` add the first
compiled-in `STATE_SCENE` reactive loop. The package-facing primitive remains
`STATE_SCENE`; the probe value `LP_GRAPH` is only the current FW0 internal host
label. `thInput` routes accepted package L/R presses through `qRuntimeEvents`,
`thRuntime` performs one bounded state update per accepted press and sends a
render request through `qDisplayCmd`, and `thDisplay` alone composes dirty rows
and publishes the six-step two-phase/three-phase waiting presentation. Content
revision advances independently from timeline revision, so compatible state
updates preserve the current combined frame and absolute deadline rather than
restarting at phase zero. Evidence `EV-HW6-20260822-P1-STATESCENE-089` recorded
`17` accepted runtime actions and `17` state changes, content/timeline revisions
`18/1`, one stable presentation ID, timeline preserve/rebase counts `18/2`, and
`11` completed STOP2 entries. No thread, queue, peripheral owner, or dynamic
allocation was added.

This is the intended normal boot slice for FW0. It is deliberately smaller than
the retained-peripheral diagnostic lifecycle: display/audio/sensor/storage/comm
diagnostic cycles are not auto-run, and the retained lifecycle report may show
only the power owner completed (`required/completed = 0x7f / 0x1`) during a
normal boot capture. That result is expected unless the diagnostic workflow is
explicitly requested.

Later FW0 PMIC/charger work extends the normal power-owner path without changing ownership: `PMIC_INT` on `PB15` records a minimal EXTI edge counter/timestamp in ISR context, and `thPower` consumes the pending edge on its bounded heartbeat path by taking the normal ADP5360 snapshot and rerunning battery/charger policy. HW6 unit 001 validates the charger/VBUS-safe PMIC interrupt profile with MCU `PB15` pull-up, guarded `EXTI15` arming after RTOS owner initialization, ADP5360 interrupt enables `0x03/0x00`, write-one-clear flag handling, and interrupt-driven `thPower` snapshot consumption. The periodic `thPower` battery snapshot remains as a fallback for non-enabled sources and long-run monitor coverage.

## HW6 Measured Owner-Path Baseline

`EV-HW6-20260731-P5-OWNERS-002` replaced the periodic power-owner diagnostic
with the first real queued owner workflow on `HW6-UNIT-001`:

- `thPower` consumed a `qSysEvents` workflow command and read the ADP5360 only
  through the mutex-backed I2C3 lease
- `thPower` sent a diagnostic frame request through `qDisplayCmd`; `thDisplay`
  alone operated SPI3/LPDMA and returned bounded completion flag `0x02`
- `thPower` then sent a tone request through `qAudioCmd`; `thAudio` alone
  operated SAI/GPDMA and `SD_MODE`, then returned completion flag `0x04`
- every queue send, wait, and acknowledgement-set operation returned success
- the deterministic display hash matched and the physical axes/corner markers
  were visible
- the 4.096 MHz/16 kHz SAI DMA path produced an audible 1 kHz tone
- `SD_MODE` and the bounded `PWR_DBG` workflow marker both returned low

The whole sequence completed in 79 ticks at 100 Hz. This proves functional
producer/consumer routing and owner-exclusive operation for these three paths.
It does not establish final message contracts or close saturation, injected
fault, cancellation, quiesce/resume, STOP2, or repeated-lifecycle behavior.

## HW6 Owner-Lifecycle Baseline and Cycle Probe

`EV-HW6-20260731-P5-OWNERS-004` proves the corrected seven-owner inactive
baseline on `HW6-UNIT-001`. The baseline completed with owner masks
`0x7F / 0x00`, no rejected transition, exact readable-register verification,
and successful terminal sensor, flash, USB, and NINA low-power actions.

- `thPower` starts its local power/PMIC transition and sends one generic
  `STABILIZE` command to display, audio, input, sensor, storage, and comm.
- Each owner performs only its own physical action and returns a dedicated
  acknowledgement bit even when its action fails. Send status, bounded wait
  status, acknowledgement flags, action status, and start/end ticks are all
  retained in the lifecycle probe.
- The expected completion and success masks are both `0x7F`. A completed owner
  is not silently rerun within the same boot.
- USB device parking is an entry action of the storage owner, not a separate
  owner. The detached baseline requires VBUS absent and verifies PCD deinit,
  USB-clock disable, and VDDUSB isolation before flash deep power-down.
- The run is debugger-gated through `g_ps_hw6_owner_sm_start_request`; ordinary
  boot creates and settles the RTOS topology without starting the physical
  diagnostic sequence.
- `PWR_DBG` bounds the requested workflow. The consolidated
  `__fw0_all_probe_prints.gdb` report includes owner transport results, final
  states, rejected transitions, device readbacks, and the transition trace.

The first lifecycle-v3 board run completed every queue send, bounded wait, and
acknowledgement in both `inactive -> active -> inactive` cycles. It also passed
the PMIC, display, audio, TMAG3001, flash/USB, and NINA hardware actions. The
only failure was the LIS2DUX12 wake action: software used the SPI-specific
`EN_DEVICE_CONFIG` command on I2C and interpreted the documented first-address
NACK as a fault.

Lifecycle-v5 passed on `HW6-UNIT-001` with the same owner order and queue
envelopes. It accepts the expected LIS2DUX12 I2C wake NACK, waits 30 ms,
revalidates `WHO_AM_I=0x47`, configures low-rate active mode with
`CTRL5=0x10`, and recommits deep-power-down at the end of each cycle. Transport
and action success remain independent.

Lifecycle-v6 kept the same queue envelopes and replaced the remaining raw
TMAG3001 owner-local register sequence with the HW6-native `ps_dev_tmag3001`
wrapper under the input-owner I2C3 lease. It passed identity, active XYZ
continuous configuration (`SENSOR_CONFIG1=0x70`, `DEVICE_CONFIG2=0x02`),
expected first sleeping-device wake status `5` plus retry success, and terminal
sleep recommit across both cycles.


Lifecycle-v7 kept the same queue envelopes and replaced the display owner direct LCD calls with the `ps_dev_ls013b7dh05` wrapper. It passed the known full-frame card through `thDisplay` with driver API/init/state/ops/last `1 / 0x0 / 2 / 3 / 0x0`, hash `0x360cda71`, DMA done `1`, and zero SPI/DMA errors. Evidence is preserved in `EV-HW6-20260801-P5-DISPLAY-008`.

Lifecycle-v8 kept the same queue envelopes and replaced the audio owner direct SAI/GPDMA and `SD_MODE` calls with the `ps_dev_audio` wrapper. It passed the known speaker tone through `thAudio` with driver API/init/state/ops/last `1 / 0x0 / 3 / 3 / 0x0`, SAI/sample/tone `4096000 / 16000 / 1000`, `SD_MODE` `0 / 1 / 0`, start/stop `0x0 / 0x0`, and zero SAI/DMA errors. Evidence is preserved in `EV-HW6-20260801-P5-AUDIO-009`.

The cycle performs real device work: flash release/re-identification/deep
power-down, TMAG wake/configure/sleep, LIS deep-power-down exit/low-rate setup/
deep-power-down, NINA DSR wake/AT/DSR STOP, display DMA, audio DMA, and PMIC
snapshots. It records both the ten-FSM active boundary mask and final inactive
boundary mask for each cycle. It does not enter STOP2 and does not yet prove
fault recovery, cancellation, saturation, current, or production message
contracts.

---

## Validation Evidence Required

Before runtime host feature work:
1. queue producers and consumers verified
2. mode and power event paths verified
3. quiesce/resume barrier verified
4. overflow and timeout behavior verified

Tracealyzer snapshot evidence should be used where practical to prove owner-thread scheduling, queue wake/block behavior, and quiesce/resume ordering. Snapshot capture policy is defined in [[Tracealyzer_Snapshot_Evidence_Contract]].
