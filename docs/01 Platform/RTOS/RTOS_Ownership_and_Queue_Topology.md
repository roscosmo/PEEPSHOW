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
| `thRuntime` | runtime host manager dispatch; tracks active runtime class, execution semantic, lifecycle, shell/installer return context, and symbolic runtime clock intent requests through `thPower` |
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

---

## Event Flag Groups

Recommended groups:
- `egMode` runtime class and mode state
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

Current FW0 stack sizing is compile-time tunable through `KNOB_RTOS_DEFAULT_STACK_BYTES`, `KNOB_RTOS_POWER_STACK_BYTES`, and `KNOB_RTOS_STORAGE_STACK_BYTES`. `thPower` deliberately has a larger stack budget than the default owner stack because it owns PMIC policy plus HAL clock-policy transitions during USB MSC, STOP2, and shutdown paths. The RTOS probe records configured stack bytes plus ThreadX stack start/end/current/high-water pointers for each owner so stack damage can be separated from normal storage or USB reclaim failures.

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

The measured RTOS probe values for the original v3 boot slice were `init/runtime complete = 1 / 1`, `boot power/display = 1 / 1`, owner started mask `0x1ff`, queue self-test mask `0x1ff`, event self-test mask `0xf`, and init status `0x0`. FW0 evidence `EV-HW6-20260812-P1-CLOCKBOOT-038` adds the storage-owned USB boot-park command: storage queue send/wait succeeded (`0/0`), the ACK flag was set, HOME rendered, USB clock/VDDUSB/HSI48 were off, and no long storage action ran during normal boot. HW6 evidence `EV-HW6-20260813-P1-RUNTIME-044` validates the first `thRuntime` scaffold: normal boot reports `SHELL / REACTIVE / RUNNING` (`class/exec/lifecycle = 1/1/2`), package MSC entry reports `INSTALLER / REACTIVE / RUNNING` (`1/5/1` class prev/current/return), a valid package prompt remains in `INSTALLER`, and install-stub completion returns to `SHELL` with no runtime error (`installer enter/done/err = 2/2/0`). HW6 evidence `EV-HW6-20260813-P1-RUNTIMECLOCK-045` validates the first `thRuntime` clock-intent requester hook: runtime commands request `REACTIVE_TRANSACTION_ACTIVE` through the power queue, `thPower` resolves and ACKs with requester-specific clock flags/status, runtime releases its requester slot after the command, and the validated idle capture returned requester cap `RT=0x0` with `STOP2 ready=1`. HW6 evidence `EV-HW6-20260813-P1-UICLOCK-046` validates the matching `thUI` clock-intent requester hook: HOME dispatch and deferred `NAV_MENU` both request `REACTIVE_TRANSACTION_ACTIVE`, `thPower` ACKs through the UI requester slot, `thUI` releases after the bounded transaction, and the validated captures returned requester cap `UI=0x0` with `STOP2 ready=1`. HW6 evidence `EV-HW6-20260813-P1-DISPLAYCLOCK-047` validates the first `thDisplay` display-transfer clock-intent requester hook: the boot clear-hold transaction requests `DISPLAY_TRANSFER_ACTIVE`, `thPower` blocks STOP2 while requester cap `D=0x8` is active, `thDisplay` releases after completion, and the requester slot clears with `STOP2 ready=1`.

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
