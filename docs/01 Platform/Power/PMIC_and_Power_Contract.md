# PMIC and Power Contract

This document defines the active HW6 Platform contract for the ADP5360 PMIC, battery policy, VBUS detection, shipping-mode handling, and system power state ownership. HW5 measurements remain historical evidence only; retained behavior must be revalidated on HW6 before it is published as a granted target capability.

Related:

- [[Power_Architecture_Index]]
- [[Power_and_Sleep_Policy]]
- [[Button_Input_Contract]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Power_Rails]]
- [[HW6_Wake_Sources]]
- [[HW6_Revalidation_Matrix]]

---

## Hardware

PMIC: `ADP5360`.

Current HW6 battery planning basis, pending incoming-board validation:

- Bring-up battery simulator / source meter: Nordic Power Profiler Kit II (`PPK2`) or equivalent controlled source.
- Previous cell family: `LIR2540` rechargeable coin cell.
- Current cell family: `303040` flat LiPo pouch cell.
- Reported cell marking: `3.7 V` nominal, `450 mAh`, `1.665 Wh`.
- The marked energy is internally consistent: `3.7 V x 0.450 Ah = 1.665 Wh`.
- Confirmed charge terminal voltage: `4.20 V`.
- The cell is accepted for charging at up to `1 C` (`450 mA`), but the
  ADP5360 charger is limited to `320 mA`; therefore the Platform charge-current
  ceiling is `320 mA` (`0.711 C`) before input-power and thermal derating.
- The cell has two electrical leads. A board-mounted `100 kOhm` NTC is held in
  physical contact with the pouch and is the ADP5360 temperature input.
- The current development cells include protection for handling/assembly. That
  protection is not part of the Platform safety contract: production cells may
  be unprotected, and ADP5360 battery protection must remain configured and
  validated as the authoritative protection path.
- Capacity remains a label/specification value until measured. Charge
  termination current, USB/VBUS input-current policy, and final production-cell
  integrated-protection status remain pending.

Connections carried into the final HW6 design/IOC intent and requiring board-level revalidation:

- ADP5360 is connected to MCU I2C at address `0x46`.
- ADP5360 interrupt is `PMIC_INT` on `PB15` / `EXTI15`.
- `BTN_START` is connected to the ADP5360 `MR` path.
- VBUS can be detected reliably through ADP5360 status on HW6 unit 001.
- `USB_OTG_FS_VBUS` is on `PA9`, but HW6 unit 001 measures only about `1.2 V` on PA9 from the fitted `47 kOhm` / `15 kOhm` USB divider. Treat PA9 VBUS as diagnostic-only for this board revision, not as authoritative policy input.
- On HW6 unit 001, `PMIC_INT` requires the MCU-side `PB15` pull-up. Treat the
  ADP5360 interrupt output as an active-low interrupt line that is not
  externally pulled up on the validated board.

Per [[Platform_Hardware_Abstraction_Contract]], the PMIC driver uses the public 7-bit address `0x46`; STM32 HAL shifted-address handling is hidden inside the `ps_hw_i2c3` layer.

---

## Ownership

- `thPower` owns PMIC configuration, battery policy, charger state, VBUS classification, sleep entry, wake classification, and power fault escalation.
- `thInput` owns START button edge/hold classification and publishes shipping-intent events to `thPower`.
- Other owners publish activity blockers or quiesce acknowledgements; they do not directly enter STOP or change power policy.
- The Reference Game and Engine express power intent only.

---

## Power Policy Decisions

- Low battery warning threshold warns, reduces optional load, and prepares the system for possible shutdown.
- Low battery forced-sleep threshold may enter STOP/static low-power behavior only while the pack is still recoverable without a latched shutdown.
- Critical battery threshold is a controlled shutdown path: firmware saves/quiesces what it can, then requests ADP5360 shipment mode by software when the critical-battery software-shipment gate is enabled.
- ADP5360 hardware BAT_UV / ISOFET protection is the lower emergency fallback, not the normal PeepOS critical-battery policy.
- Firmware must prevent restart loops by using a restart-allow threshold above the critical-shutdown threshold and by allowing charger/VBUS-present recovery handling.
- The device may run normally while charging.
- Flashing/install mode is the exception: charging does not imply normal runtime use while the device is in flashing mode.
- All main power rails may remain active; power savings come primarily from MCU sleep, peripheral low-power modes, local enables, and device-specific shutdown/deep-power-down.

---

## START / Shipping-Mode Rule

START is both a normal system button and the ADP5360 `MR` shipping-mode path.

Rules:

- firmware may detect START hold intent early
- firmware should save state and quiesce before the hardware shipping threshold
- firmware may display warning/countdown UI
- firmware must not assume it can prevent shipping mode once the ADP5360 threshold is reached
- START shipping intent and battery-critical shutdown are separate state-machine paths that may share the final ADP5360 shipment-mode primitive.
- battery-critical software shipment is allowed only after controlled save/quiesce policy has run and only when its explicit Platform gate is enabled.
- battery-critical software shipment must use a restart-allow threshold above the critical threshold so START or battery rebound cannot cause a shutdown/wake loop.

### HW6 FW0 Shipping-Mode Enable Status

On HW6 unit 001, FW0 normal boot now enables the ADP5360 hardware START/MR
shipping-mode path through `thPower`. The power owner sets Supervisory Setting
register `0x2D`, bit `1` (`EN_MR_SD`), then takes the normal PMIC status
snapshot. Evidence `EV-HW6-20260810-P5-BOOT-021` records:

- RTOS init/runtime complete `1 / 1`
- normal boot power/display complete `1 / 1`
- ADP driver API/init/MR/state/ops/last `4 / 0 / 0 / 2 / 2 / 0`
- PMIC snapshot command/complete/success `0 / 1 / 1`
- ADP register read addresses `00 29 2a 2b 2c 2e 2f`, values
  `10 31 18 18 13 00 07`
- I2C lease, HAL transfer, and lease release statuses all `0`
- identity, rails, and fault checks all pass

This measured result means the firmware enables the ADP5360 option that lets a
12-second START/MR hold enter shipment mode, which protects the cells while the
full low-power policy is still under development.

This does not close the full shipping-mode product behavior. Firmware now has
target-validated START hold classification through the prep, warning,
and imminent scaffold, and HW6 unit 001 confirmed ADP5360 shipment entry
after a long START/MR hold. Firmware still needs warning/countdown UI,
save/quiesce behavior, enabled automatic shipment tests, final wake/recovery UX, and
first-boot/no-settings policy. The hardware threshold must still be treated as
final once reached.

The current FW0 START scaffold routes input-owned hold events to `thPower`, records them in the power state-machine probe, preserves the prior active return state, enters existing `PWR_SHIP_PREP` / `PMIC_SHIP_PENDING` states, asks RTOS system-action admission to suspend an active package runtime before physical owner quiesce, asks every physical owner to quiesce through its own ThreadX queue with bounded ACK waits, and asks `thUI` to show a plain `SHUTDOWN` scaffold page. Release before shipment requests cancel, resumes only a runtime suspended by START-shutdown admission, and returns the UI to the prior page. Battery-critical and boot-low-battery shipment prep share the same admission-before-quiesce shape, but they do not auto-resume runtime. FW0 also contains a guarded power-owner primitive for ADP5360 software shipment entry by writing Shipment Mode register `0x36 = 1`. START imminent may request that primitive only when `KNOB_POWER_START_SOFTWARE_SHIP_ENABLE` is true; the generated default is false, so normal bring-up START tests do not software-enter shipment. HW6 unit 001 validation confirmed the owner quiesce barrier on START prep with required/send/ACK/success/failure masks `0x7e/0x7e/0x7e/0x7e/0x0`, all owner action statuses `0x0`, and power/PMIC held in `PWR_SHIP_PREP` / `PMIC_SHIP_PENDING` (`8/8`). Earlier validation confirmed the default-off START software shipment gate stayed closed during a full prep/warning/imminent/release sequence: software shipment enable/request/skip was `0/0/1`, PMIC software shipment request count stayed `0`, and the UI/display returned to HOME after release. HW6 evidence `EV-HW6-20260814-P1-POWERADMIT-054` adds runtime-admission proof for START-shutdown prep and START-cancel resume.

HW6 unit 001 evidence `EV-HW6-20260811-P1-SHIP-032` validates the underlying software shipment primitive itself. A debugger-only manual request set `g_ps_hw6_pmic_software_ship_request = 1` while the device was in normal active power state `2 / 3`; after continue, the target lost power/connection with `Remote failure reply: E31`, and the device restarted only after a START press. This proves the firmware-owned ADP5360 `0x36 = 1` path can enter shipment mode without waiting for the 12-second hardware MR threshold. It does not validate automatic START-imminent, critical-battery, or boot-low-battery software shipment gates.

This path is a normal boot power-owner action. It is not part of the retained
peripheral diagnostic lifecycle and must not require display, audio, sensor,
storage, or communication diagnostic cycles.

---

## Critical-Battery Shipment And Restart Gate

Critical-battery shutdown is a PeepOS-controlled sequence while firmware is still alive enough to make a safe decision.

As of the 2026-09-15 integration checkpoint, normal HW6 FW0 defaults enable
`power_critical_software_ship_enable` and `power_boot_low_battery_ship_enable`.
The controlled bench tests established low-boot and runtime-critical shipment,
shortened unattended battery-wake shutdown, synthetic failure backoff and
valid-voltage recovery. The user also confirmed the promoted normal build's
3.4 V boot shutdown, shortened unattended critical shutdown after approximately
15 seconds, and normal START boot after restoring 3.8 V. Permanent physical
failures, exact long-interval cadence and charger recovery remain unqualified.
`power_start_software_ship_enable` remains false. Voltage thresholds and battery
wake intervals are unchanged: warning 3500 mV, critical 3300 mV, restart 3600 mV,
healthy sleep checks 30 minutes and warning/failure checks 60 seconds.
Explicit gates-off diagnostic builds remain possible, but must never be mistaken
for battery-protected normal firmware. The fault-injection helpers deliberately
refuse the enabled normal build; do not clear their guards to run them.

Rules:

- On critical battery, `thPower` must request owner quiesce/save through bounded Platform-owned hooks before any software shipment request.
- If the critical-battery software-shipment gate is disabled, firmware must record and expose that shipment would have been requested, but it must not write ADP5360 Shipment Mode register `0x36`.
- If the gate is enabled and owner preparation succeeds, `thPower` may request ADP5360 Shipment Mode register `0x36 = 1`. Exhausted preparation uses the checked fault-wait path; it does not bypass a failed owner to write shipment mode.
- The ADP5360 hardware BAT_UV / ISOFET cutoff remains a lower emergency protection path if firmware cannot act in time.
- Startup must read battery/VBUS state before enabling display-intensive work, audio, vibration, radio, switched rails, package runtime, or installer behavior.
- Battery threshold decisions must use a valid decoded VBAT measurement; successful I2C reads with all-zero/raw-invalid fuel-gauge voltage must be treated as unknown and must not trigger critical shutdown.
- ADP5360 fuel-gauge VBAT reads must not be trusted until `thPower` has configured fuel-gauge active mode and requested an SOC/VBAT refresh.
- If VBUS/charger recovery is present, startup may remain in a charge/recovery shell even below the restart-allow threshold.
- If VBUS is absent and battery is below the restart-allow threshold after a shipment wake or low-battery reset, firmware must not continue normal runtime. With the boot-low-battery shipment gate disabled, it records a would-ship result; with the gate enabled, it re-enters software shipment.
- The boot/restart battery check remains pending until the first valid fuel-gauge VBAT sample is available. It must not treat the initial unavailable fuel reading as permission to enter normal runtime.
- While boot/restart is blocked without VBUS, the UI may show a plain low-battery recovery page. With VBUS present and VBAT below the restart-allow threshold, the UI may show a plain charging recovery page. Neither page is a normal shutdown-cancel target; both clear only when `thPower` reports VBAT at/above the restart-allow threshold.
- The restart-allow threshold must be higher than the critical-shutdown threshold.

Battery shutdown preparation now has independent attempt state, rather than
using the ship-prep ownership flag or a lifetime boot-block counter as proof of
completion. `power_battery_shutdown_prep_attempts` defaults to three total
attempts and `power_battery_shutdown_prep_retry_ms` to 1000 ms minimum spacing
from failed completion. These are bounded retries of admission/owner quiesce,
not an increased periodic battery polling rate. Existing snapshot cadence and
all voltage thresholds remain unchanged.

Each retry requires a subsequent valid critical reading, or a valid blocked
no-VBUS boot reading. Failed/zero VBAT readings do not authorize an attempt.
Successful preparation issues one gated battery-owned shipment request. The
final owner call reports its result back to battery policy. A returned failure
invalidates prepared state and permits another attempt only after the existing
retry spacing and a subsequent valid qualifying reading. Every retry repeats
admission and owner quiesce; the same total preparation-attempt budget bounds
the whole episode, including attempts following a failed shipment command.
Manual/START shipment requests remain separate one-shot requests.
Exhaustion retains the last preparation failure and leaves shutdown preparation
owned; it must not fabricate an ACK, resume normal work, or bypass quiesce.
There is no blocking delay, spin loop or automatic unlimited retry.

Exhaustion latches battery fault-wait without replenishing shipment attempts.
`thPower` repeats admission and physical owner quiesce before fault sleep,
using the original battery reason for terminal peripheral parking. STOP2 retains
clock, RTC, GPIO, queue and final-input validation. Failed sleep attempts are
spaced by `power_battery_sleep_retry_ms`, also the maximum fault-read interval.
Fault wakes restore clocks/timebases but do not resume normal owners, package
timers, animation or input delivery. Every real wake requests a fresh reading.
Recovery requires a valid reading at/above restart-allow and successful physical
owner resume; buttons and VBUS alone cannot clear the episode. Package work
remains suspended for explicit recovery. If safe sleep cannot be established,
the failure remains visible; firmware must not claim low-power residency.
Independent hardware protection is still required.

The explicit FW0 fault-wait bench request is admitted once per boot only with
all shipment gates disabled, fresh qualifying battery data, absent/agreeing
VBUS detection and a completed battery preparation. `thPower` restarts that
test episode and injects a failed preparation return only after the actual
admission/quiesce call succeeds. Real owner results are retained separately;
no physical ACK is fabricated, and failed real preparation is not relabelled as
successful injection. The existing retry budget and spacing still determine
exhaustion. Shipment requests are discarded while this test is active.

Only this armed test caps the battery wake interval with
`power_battery_fault_test_wake_ms` (15000 ms). Its first classified button wake
opens one `power_battery_fault_test_inspect_ms` (60000 ms) awake inspection
window. RTC wakes do not open the window; later buttons cannot extend it.
The window uses running ThreadX time, so a debugger halt freezes its countdown.
Package suppression and battery monitoring remain active. Current measured
during inspection is not fault-sleep current. Healthy recovery ends the test
but preserves its evidence; another injection requires an intentional reset.

FW0 fault-test mode 2 uses the same admission and preparation exhaustion, then
consumes one SENSOR-owner result injection during the first fault quiesce.
The physical owner runs its real quiesce first. Only a real HAL_OK becomes a
reported HAL_ERROR; a real failure is preserved and consumes the injection
without claiming a synthetic refusal. Normal owner result/mask publication and
ACK delivery are unchanged, so receipt of the ACK does not permit sleep when
the reported action failed. There is no withheld ACK or deliberate bus fault.
The existing `power_battery_sleep_retry_ms` backoff is unchanged. Battery checks
continue while awake, and the next attempt must pass a new complete barrier.
Recovery clears any pending injection. Retained fault-test API 2 data separates
the real result, synthetic refusal, next attempt timing, intervening WFI and
successful due reads. This qualifies transient refusal handling, not safe
parking of permanently broken hardware.

The separate `g_ps_hw6_battery_shutdown_probe` (API 2) reports reason, attempts,
prepared, exhausted, last status and next kernel tick for the current episode.
It also reports battery shipment pending, actual owner-call attempts, returned
failures, latest result and first failure. A queued request reports NOT_RUN,
not successful execution. A returned HAL_OK is command completion, not proof
that the rail fell. No repeat is scheduled from HAL_OK alone. The actual
physical-shutdown test still requires current/rail evidence.
It resets on initialization and existing valid recovery paths: runtime voltage
above warning, boot voltage at/above restart-allow, or boot VBUS charge recovery.
Warning/unknown samples do not replenish the attempt budget. After charge
recovery, removing VBUS while still boot-blocked permits a new preparation
episode; a historical diagnostic counter no longer prevents it.

For voltage recovery from battery-owned `PWR_SHIP_PREP`, recover
`PMIC_SHIP_PENDING` to `PMIC_MONITOR` before clearing battery ownership. The
same cleanup applies when a blocked boot enters VBUS charge recovery, before
selecting the charging state. Do not clear a START-only pending shipment merely
because a healthy voltage sample arrived. This is software-state cleanup, not
automatic package resume or evidence that physical shipment occurred. Native
regressions exercise the actual power/PMIC transition tables. On 2026-09-14,
runtime recovery at measured 3782 mV returned policy OK and power/PMIC to 2/3,
cleared preparation tracking and issued no shipment request.

The battery quiesce timing probe (API 3) retains the latest critical/boot-low
barrier, per-owner dispatch/wait-completion ticks and display clock queue/ACK
completions observed during that barrier. Normal STOP2 barriers do not overwrite
it. `__fw0_battery_quiesce_timing_prints.gdb` prints the record. Each preparation
retry replaces the latest record. A separate first-failure record survives all
later barriers and recovery until reset. It freezes the first completed battery
barrier with a failed result or an observed display clock queue/ACK failure.
Both records include per-owner send/ACK/flags/action results, final barrier
status and display/storage clock grant/release results. Owner action results
are sampled when that owner's wait returns; late completions cannot alter the
frozen record. Admission failures before the barrier are outside this probe.
API 3 also samples boot/calibration progress, an outstanding storage clock
transaction at barrier begin and storage dispatch/completion, and joystick
owner/driver states and ready/identity/sleep-write results around input quiesce.
These copy existing software probes without additional peripheral reads.
Storage wait=1 means a clock transaction is outstanding; it does not by itself
prove a clock fault. Input results without an ACK may be pending or historical.
The final result is recorded after storage-clock cleanup. These timestamps include scheduling and waits;
they do not measure isolated CPU time. The last display clock result can replace
an earlier result, while the failure count remains cumulative for that barrier.
No queue, timeout, clock-transition or acknowledgement behaviour is changed by
this observer. Do not halt during the measured preparation.

The 2026-09-14 completed target capture established a circular wait: thPower
awaited display quiesce while display awaited a clock reply from thPower.
Preparation took 1000 ticks (10 seconds) and reported success after a display
clock ACK timeout. That result is not a clean preparation pass.
Battery-critical and boot-low barriers now use the existing power/display
handoff: thPower grants display transfer clocks directly before waiting for
display, satisfies an outstanding clock wait, and holds the grant while display
finishes and processes quiesce. The existing queued-request suppression prevents
the superseded request from restoring transfer capabilities later. Cleanup ends
the handoff and releases the display grant; grant/release failure rejects
preparation. Normal sleep and START barriers remain unchanged. Owner ACKs are
still required; this does not synthesize a display-quiesce acknowledgement.
Focused native regression tests pass. Runtime target revalidation at measured
3175 mV completed preparation in ticks 479..482 (30 ms), with all owner actions
and ACKs successful and an outstanding display clock request completing with
zero failures. Physical shipment remained gated off. Low-voltage boot at
measured 3374 mV subsequently required two attempts; only the successful second
attempt was retained by API 1. The new first-failure capture is intended to
identify that unresolved first attempt, not to declare boot preparation fixed.
The subsequent retained first failure showed storage ACK timeout from tick
260 to 1260 and input action HAL_ERROR despite its successful ACK. The second
barrier at tick 1384 completed successfully and the user saw the warning appear.
The API-3 capture confirmed boot/calibration overlap: calibration was started
but unresolved, storage was waiting for a clock reply at barrier begin and
dispatch (request start tick 28, capabilities 0x2), and thPower waited for its
quiesce ACK until timeout. The joystick hardware operation actually succeeded:
ready/identity/sleep-write and driver status were zero, terminal sleep was
committed, but owner state JOY_OFF rejected JOY_EV_QUIESCE. The retry accepted
the existing sleep proof. This was a state-handling error, not an I2C failure.

Battery-critical/boot-low preparation now reserves the storage flash clock even
before FLASH_READY. A power-owned storage handoff answers a pending clock
request and serves flash-clock requests/releases locally while the real owner
barrier runs. The grant remains held until barrier cleanup; it does not grant
USB/MSC capabilities or manufacture storage's quiesce ACK. A grant failure is
retained for the waiting caller even if cleanup subsequently succeeds. The
superseded queued request is consumed without a second ACK or clock change.
Grant, owner send/ACK/action and cleanup failures still reject preparation.
Normal sleep/START storage handoff behaviour is unchanged. After successful
hardware sleep, input now handles JOY_OFF/JOY_SUSPENDED consistently with the
cached sleep-proof path; other states still require their valid transition.
These corrections are native-tested and build-verified. Low-boot target
revalidation at measured 3373 mV completed on attempt 1 in ticks 258..261
(30 ms), with all owner actions/ACKs successful and no first failed barrier.
The pending storage clock wait cleared before storage dispatch; its grant and
release succeeded. Input's hardware sleep and owner action both succeeded.
The user confirmed the warning appeared directly after boot and recovery after
raising voltage. Recovery is visually confirmed in this run; no post-recovery
probe was supplied. Shipment remained disabled, so physical shutdown and
discharge protection are not established by this pass.

That preparation fix did not change START handling or retry the final PMIC
shipment write. The later API 2 result-handling increment above adds bounded
re-preparation after a returned write failure; the driver primitive is unchanged.
Automatic critical/boot shipment remains disabled in normal builds.
Physical shipment failure handling and a bounded fallback after exhausted
preparation still require qualification before enabling automatic protection.
Native tests compile the actual preparation and battery evaluation functions
with fake admission/quiesce boundaries, for gates both off and on. They cover
admission failure, quiesce timeout, spacing, exhaustion, invalid measurements,
single request after success, recovery, VBUS removal and kernel tick wraparound.
Controlled-voltage automatic low-boot and runtime shutdown subsequently passed
in the isolated test build, including a shortened unattended RTC wake to
shipment at 4.8 uA; see [[HW6_Battery_Shutdown_Validation]]. Physical failure
injection, permanent-failure low-power behaviour and independent hardware
protection remain unqualified.

Verification for this preparation fix: 107 firmware tests and the Debug build
pass; generated target-profile and whitespace checks pass. Build usage is
RAM 552808 bytes, ROM 879664 bytes and SRAM4 15480 bytes. The existing V2 runtime
stack check still reports 2432/4096 bytes including reserve; this is not a
power-thread stack high-water measurement or physical shutdown evidence.

Provisional FW0 thresholds, pending HW6 measurement and UX review:

| Threshold / gate | Provisional value | Purpose |
|---|---:|---|
| battery monitor cadence | `1000 ms` | periodic `thPower` PMIC snapshot while FW0 monitor scaffold is active |
| warning threshold | `3500 mV` | show warning and reduce optional load |
| critical controlled-shipment threshold | `3300 mV` | save/quiesce then request software shipment when enabled |
| restart-allow threshold | `3600 mV` | prevent battery rebound from restarting normal runtime |
| critical software-shipment gate | `false` | keep bring-up tests from powering off unexpectedly |
| boot-low-battery shipment gate | `false` | record would-ship until the boot path is validated |

HW6 FW0 target evidence now validates the runtime policy path at one normal point, one warning point, one critical point, one runtime recovery point, the no-VBUS boot/restart block, the VBUS-present boot charge-recovery path, the START runtime-admission/cancel-resume scaffold, the START owner-ACK quiesce barrier, the pre-STOP sleep-prep owner-ACK scaffold, the manual STOP2 START-wake scaffold, and the shared ADP5360 software shipment primitive. The no-VBUS boot-gate case used a controlled source: `3270 mV` held policy state `BOOT_RESTART_BLOCKED`, kept power/PMIC in `PWR_SHIP_PREP` / `PMIC_SHIP_PENDING`, showed UI/display shutdown state `LOW_BATT_BOOT`, and recorded a default-off software-shipment skip. Raising the source produced `3707 mV`, cleared the boot gate, returned power/PMIC to `PWR_ACTIVE_LP` / `PMIC_MONITOR`, and returned UI/display to HOME. The VBUS-present case used the restart threshold forced to `4200 mV`: PMIC-read `3968 mV` selected `BOOT_CHARGE_RECOVERY`, suppressed HOME, showed UI/display shutdown state `LOW_BATT_CHARGE`, kept shipment requests at zero, and PMIC entered `PMIC_CHARGING`. Restoring the restart threshold to `3600 mV` with PMIC-read `4049 mV` cleared the boot gate and returned UI/display to HOME while PMIC stayed charging. `EV-HW6-20260811-P1-SHIP-032` then proved a manual power-owner request can write ADP5360 `0x36 = 1` and place the device in shipment mode. Threshold values remain provisional until UX, current, automatic gated software-shipment paths, and persistent save evidence are complete. `EV-HW6-20260811-P1-SLEEP-034` proves sleep-prep can enter `PWR_SLEEP_PREP`, collect owner ACKs with masks `0x7e/0x7e/0x7e/0x7e/0x0`, intentionally skip STOP entry, and recover to `PWR_ACTIVE_LP`. `EV-HW6-20260811-P1-STOP2-035` proves the follow-on manual path can enter real STOP2 after the same owner-ACK barrier, wake from START on PA4, restore clocks, recover the power FSM to `PWR_ACTIVE_LP`, collect baseline post-wake owner ACK/liveness proof with inactive/parked owners, and complete a staged active-owner STOP2 pass where audio/input/display/sensor/comm are active, quiesced, STOP2-entered, then resumed or confirmed live after wake. Storage/flash were intentionally excluded from the staged active-owner pass to avoid repeating flash scratch/erase/write tests; production automatic STOP admission, wake classification, tick compensation, LPBAM, current, repeated cycles, and fault-injection remain open.

---

## Planned PMIC Monitoring Schedule

Status: optional-work scheduling is not implemented. The read-group foundation
and periodic STOP2 battery deadline are implemented as described below, with one
shortened battery wake/read hardware-confirmed. FW0 still performs a full snapshot when
the `1000 ms` monitor period is due, with additional boot, interrupt and explicit
diagnostic requests. Checking whether the period is due is not a hardware read.
The current snapshot performs 24 single-register reads and up to two flag-clear
writes. It runs synchronously in `thPower`, so even an already-acknowledged clock
request can wait for this work before its requesting thread runs.

The next monitoring increment separates these workloads without transferring
PMIC ownership away from `thPower`:

| Workload | Trigger and scheduling rule | Required validity |
| --- | --- | --- |
| Battery safety | Retain the existing 1000 ms active-monitor target initially; service relevant power events promptly. Faster checks near thresholds or around load changes require an explicit policy and measurement. | Valid VBAT and the presence, fault and external-power information needed for the decision; slow SOC must not substitute for voltage safety. |
| Charger and VBUS events | Interrupt-driven status refresh, with a bounded periodic backstop for missed events. Coalesce requests without discarding unrecorded flags. | Record observed flags before write-one-to-clear; publish decoded status only from successful reads. |
| SOC and UI telemetry | Slower, independently scheduled updates. UI and package consumers use the published cache rather than initiating synchronous PMIC reads. | Value, last-success time, age and validity are visible to consumers. An overdue value is not a fresh measurement. |
| Identity and configuration verification | Mandatory at boot, after configuration changes and after recovery; a separate, slower integrity audit thereafter. | Boot/recovery admission still requires verified identity and relevant rail, charger and gauge configuration. Ordinary telemetry cannot clear a failed verification. |

The slower telemetry, backstop and audit periods are not selected by this
design. They must become separately described knobs, with evidence supporting
their defaults. Do not silently repurpose the current monitor-period knob to
mean all of these schedules. Battery voltage can change quickly under load even
when SOC changes slowly.

### Freshness And Failure Rules

- Each group publishes its last successful sample time, latest attempt status,
  validity and a defined maximum usable age. Target update period, maximum usable
  age and permitted scheduling deferral are separate policy values.
- Publish a group only after its required reads complete successfully. Do not
  present a mixture of old and new fields as one current successful sample.
  Preserve the last good sample for diagnostics while exposing failure/staleness.
- Decisions using multiple groups must check every required group's validity
  and age. Required freshness limits must be specified before implementing that
  decision; this document does not establish a safe maximum age from one trace.
- Missing, failed or over-age safety information enters the documented
  unknown/recovery policy. It must not silently permit a new high-load operation
  or be interpreted as zero battery voltage.
- The present snapshot's overall success includes configuration verification.
  Splitting reads therefore requires explicit separate verification and sample
  validity, not merely removing configuration reads from the success test.

### Bounded Work And Sleep

- Use bounded owner jobs and service pending requests between safe transaction
  boundaries. No extra thread, cross-owner peripheral access, hidden retry loop
  or arbitrary sleep is introduced to simulate prioritisation.
- Routine telemetry and integrity audits may defer during bounded interactive
  work, but retain their original deadline and a maximum deferral. Repeated input
  cannot continually restart their deadlines. Safety and critical power events
  cannot be postponed merely to improve animation or button latency.
- A bus transaction remains indivisible, with its existing ownership and clock
  protections. Grouped register reads may reduce transaction overhead later;
  they do not replace the scheduling and freshness policy.
- Do not wake from STOP2 solely to refresh cosmetic SOC/UI data or perform a
  routine configuration audit. Overdue optional work is considered on natural
  wake; freshness accounting must include elapsed STOP2 time.
- The existing one-second owner-loop monitor is not a promise of a one-second
  RTC wake. Any required asleep safety sampling deadline must be explicitly
  designed with the power/sleep policy and shared RTC arbitration, then measured
  on hardware. Do not add such a wake implicitly while splitting the snapshot.

### Implementation And Acceptance

First define the read groups, verification barriers and published freshness
state; then schedule bounded jobs and tune optional periods. Preserve PMIC
interrupt record/clear ordering, boot battery admission, charger/VBUS reporting,
critical-battery policy and recovery behaviour throughout.

Acceptance requires traces showing actual register work and request latency,
including sustained input and simultaneous PMIC events; failure/stale-sample
tests; and hardware checks of boot, charger connect/disconnect, battery policy
and STOP2 wake behaviour. Measure current and energy as well as responsiveness.
No performance or battery-safety improvement is established by this design alone.

### Read-Group Foundation (Driver API 11)

`ps_dev_adp5360_read_groups()` accepts a mask of these groups. The full-snapshot
entry point still selects ALL, preserving the existing 24 reads in their original
order, both conditional W1C writes, lease boundaries and admission checks.
Boot, IRQ, diagnostics and periodic monitoring still use that full entry point.

| Bit / group | Read addresses in group order | Reads |
| --- | --- | --- |
| `0x1` SAFETY | `2e,2f,08,09,0a,25,26` | 7 |
| `0x2` EVENTS | `34,35`, followed by clear of successfully read, nonzero flags | 2 |
| `0x4` SOC | `21` | 1 |
| `0x8` CONFIG | `00,29,2a,2b,2c,02,03,04,07,0a,32,33,20,27` | 14 |

The duplicate thermistor-control read remains intentional compatibility work,
not a new optimisation. Configuration-group success preserves existing ID/rail
expected-value checks and requires its reads to succeed; it does not introduce
expected-value verification of every charger/gauge setting. Those writes and
boot sequencing retain their existing owners and checks. Safety-group success
preserves fault-clear and rail-good checks; battery-policy validation, including
the nonzero VBAT requirement, still applies separately.

Each attempt reports requested/valid masks and per-group status and raw values.
Unrequested fields are not valid readings. Any failed read invalidates its group;
failed W1C invalidates EVENTS; acquisition/release failure invalidates all
requested groups. Successful partial reads do not authorize boot or normal load
admission, and ordinary telemetry cannot validate CONFIG.

`g_ps_hw6_pmic_monitor_probe` records attempts, successes, last attempt status,
last-good raw group values and kernel tick timestamps. Failed attempts retain
last-good values but clear validity; unrequested groups are untouched. Owner
initialisation and PMIC configuration/shipment operations invalidate the records
before further hardware work. This is diagnostic history, not the policy cache:
its validity means successful acquisition since invalidation, not age-qualified
freshness or permission to run. Kernel timestamps must not be used to infer age
through STOP2. Maximum usable ages and a sleep-aware timebase remain part of the
next optional-work scheduling increment. The separate sleep battery deadline
below is not a general-purpose freshness API for these group records.

After rebuilding/flashing a matching ELF, `__fw0_pmic_monitor_prints.gdb` prints
these records without requesting hardware work. During healthy full monitoring,
requested/valid masks are `0xf/0xf`, statuses are zero and last-good read counts
are `7/2/1/14`. Attempt counters alone do not prove successful hardware reads.

### Periodic STOP2 Battery Deadline

Battery monitoring must not depend on a user input, package timer or charger
interrupt eventually waking the device. The shared RTC wake timer now selects
the earliest of battery, interaction and scene-timer deadlines, including when
there is no active package timer or the device is in the shell. This is an
explicit battery-safety wake, not an optional UI/SOC refresh.

Provisional knobs, requiring hardware and energy validation:

| Knob | Default | Meaning |
| --- | --- | --- |
| `power_battery_sleep_check_ms` | 1800000 (30 minutes) | Target maximum interval from a valid healthy reading to the next battery check through STOP2. |
| `power_battery_sleep_warning_ms` | 60000 | Interval after a valid reading at/below the existing warning threshold. |
| `power_battery_sleep_retry_ms` | 60000 | Maximum scheduled delay to another attempt after a failed reading. |

Only successful full-snapshot policy readings, including the existing nonzero
VBAT check, refresh the normal/warning deadline. Failed attempts use the retry
interval or an already-earlier deadline; further failures before that deadline
cannot continually push it out. Actual read work still runs through `thPower`.
The awake one-second monitor and full snapshot validation are unchanged.

`ps_battery_wake` records the remaining deadline at RTC preparation and rebases
it using measured RTC elapsed time at finish. Scene replacement and unrelated
wakes do not reset it; an actual successful battery reading on such a wake may
legitimately refresh it. Timer competition does not lose the unselected battery
deadline. A due battery check overrides the awake monitor's cadence skip even
when ThreadX ticks were stopped. RTC read/arm failures force a fresh check and
do not count as successful time accounting. RTC rounding and bounded owner work
add servicing latency; the period is not a claim of exact wall-time completion.

Battery-only RTC expiry does not queue a package interaction-timeout command or
invent a button, activation, sound or UI notification. Coincident genuine scene
or interaction deadlines are still serviced. Normal owner resume and display
reconciliation remain in place, after which a healthy settled device can return
to STOP2. RTC preparation is cleaned up on aborted sleep as well as real wake.
Shared RTC source values are NONE=0, INTERACTION=1, STATE_TIMER=2, BATTERY=3.

**Discharge protection remains incomplete until the shutdown path is qualified.**
`power_critical_software_ship_enable` and `power_boot_low_battery_ship_enable`
remain false for this wake-path increment. Low readings still enter the existing
battery policy and quiesce path, but these defaults do not guarantee physical
shipment. A controlled supply test must prove critical shutdown, failed-read
handling, VBUS-present charge recovery and restart admission before enabling
automatic shipment. Do not deliberately exhaust a cell to test this. Complete
PMIC communication failure can prevent a software shipment command; periodic
reads are not a substitute for hardware battery protection.

The one-shot `__fw0_battery_wake_enable.gdb` helper queues a shortening to 15
seconds in `thPower`; it cannot lengthen the deadline or alter voltage/charging
settings. Use the running autonomous 1234/A-B package after its timer reveal.
The shell stayed awake in the observed test and is not a suitable test setup.
Resume without input for 20 seconds, then wake normally if
needed, halt and source `__fw0_battery_wake_prints.gdb`. Firmware-held test
baselines survive debugger reconnect without reset. Require a battery RTC
expiry and a successful due-check delta, not just a timer selection or thread
run count; verify continued visuals/input and low-current return. A successful
reading before expiry can replace the test deadline, so zero expiry delta is
not a pass. Follow with a real-duration 30-minute test and controlled low-voltage
tests.

The 2026-09-14 shortened hardware test passed: expiry and successful due-read
deltas were both one, STOP2 entries two, RTC wake classifications one, clock
failures zero, and the latest valid reading was 3861 mV. The cumulative acquisition
record contained one earlier failure (six attempts, five successes); its cause
is not established. This proves one battery RTC wake and successful due reading,
not error-free lifetime monitoring, the real 30-minute interval, low-current
residency or complete discharge protection. The 60-second warning interval is
provisional pending energy measurement and shutdown policy qualification.

## VBUS Detection

VBUS may be classified from:

- ADP5360 charger/input status
- MCU `USB_OTG_FS_VBUS` on `PA9`

`thPower` records both paths for diagnostics, but on current HW6 unit 001 policy must prefer the ADP5360 VBUS view because the PA9 divider output is below the reliable digital-high range.

Disagreement between the PMIC VBUS view and MCU VBUS view is a diagnostic event until explained. It must not silently change installer/storage ownership.

VBUS classification means external USB power is present. It does not by itself prove a USB data host exists.

USB data-host classification is not derived from VBUS. FW0 diagnostics must report PMIC VBUS, MCU `PA9` VBUS, PMIC/MCU agreement, and USB protocol proof separately so charger-only attach cannot be confused with host-driven installer/export eligibility.
FW0 `thPower` PMIC snapshots may refresh the USB availability external-power fields, but no PMIC/VBUS event may mark data-host-seen or enter MSC; data-host proof remains USB protocol or MSC media traffic owned outside PMIC policy.

### HW6 FW0 Charger/VBUS Status

HW6 unit 001 FW0 evidence `EV-HW6-20260811-P1-CHARGER-027` validates the staged charger monitor path with a real cell, board-mounted `100 kOhm` NTC at room temperature, and USB/VBUS plugged.

Measured result:

- owner probe API/snapshot `9 / 1 / 1`
- power/PMIC state `2 / 4`, meaning normal active runtime with `PMIC_CHARGING`
- ADP5360/MCU VBUS agreement `1 / 1 / 1`
- charger raw status `0x22 / 0xE4`
- charger monitor read mask `0x7`, covering charger status 1, charger status 2, and thermistor-control readback
- thermistor config/status/register `0x0 / 0x0 / 0x80`
- thermistor status bits `7`, meaning thermistor OK
- charger mode/status/type/health `2 / 1 / 2 / 0`, meaning fast-charge mode, charging, fast-charge type, and good health
- fuel/policy VBAT `3732 mV`, VBUS present, battery present

This validates that FW0 configures the ADP5360 thermistor bias correctly for the board `100 kOhm` NTC and that the PMIC can enter charging state while PeepOS remains in normal active operation. It does not validate higher charge current, charge termination, full-state behavior, JEITA hot/cold/cool/warm behavior, long-run thermal behavior, or final production charging policy.

HW6 unit 001 FW0 evidence `EV-HW6-20260811-P1-CHARGER-028` validates the later boot-applied conservative charger profile with VBUS absent. The profile is applied by `thPower` during normal PMIC stabilization and then read back by the normal snapshot path. Measured result:

- owner probe API/snapshot `10 / 1 / 1`
- power/PMIC state `2 / 3`, meaning normal active low-power runtime with `PMIC_MONITOR`
- ADP5360/MCU VBUS agreement `0 / 0 / 1`
- charger profile write status `0x0`
- charger profile read mask `0x1f`
- charger profile addresses `0x02 / 0x03 / 0x04 / 0x07 / 0x0A`
- charger profile values `0x81 / 0x82 / 0x29 / 0xAC / 0x80`
- charger profile read statuses all `0x0`
- thermistor-control readback `0x80`
- fuel/policy VBAT `3759 mV`, VBUS absent, battery present
- PMIC interrupt counters stayed `0 / 0 / 0`, so this capture did not exercise a PMIC interrupt edge

This validates conservative boot-applied register ownership and exact readback for `0x02`, `0x03`, `0x04`, `0x07`, and `0x0A`. PMIC_INT event behavior was validated later in `EV-HW6-20260811-P1-CHARGER-029`. This capture does not validate charge-current promotion, charge termination, JEITA zone behavior, or long-run thermal behavior.

HW6 unit 001 FW0 evidence `EV-HW6-20260811-P1-CHARGER-029` validates the
PMIC interrupt path after the conservative charger profile. The ADP5360
interrupt output is active low and, on the measured HW6 board, needs the MCU
internal `PB15` pull-up. Firmware therefore configures `PB15` with pull-up,
holds `EXTI15` disarmed during early Cube/HAL startup, then explicitly arms the
interrupt after RTOS owner services are initialized. This avoids a boot-time
interrupt entering ThreadX startup before queues and flags exist.

Measured result:

- owner probe API/snapshot `12 / 1 / 1`
- normal active power state with PMIC monitor state `2 / 3`
- PMIC interrupt configuration status `0x0`
- PMIC interrupt registers `0x32 / 0x33 / 0x34 / 0x35`
- PMIC interrupt register values `0x03 / 0x00 / 0x00 / 0x00`
- PMIC interrupt read statuses all `0x0`
- PMIC interrupt flag clear mask `0x03`, clear statuses `0x0 / 0x0`
- ISR edge/consumed counters `1 / 1`
- `thPower` pending/snapshot/status `1 / 1 / 0x0`
- VBUS absent agreement remained `0 / 0 / 1`
- conservative charger profile readback still matched `0x81 / 0x82 / 0x29 / 0xAC / 0x80`

Rules:

- PMIC interrupt enables are applied only by `thPower`.
- FW0 enables `INTERRUPT_ENABLE1=0x03` and `INTERRUPT_ENABLE2=0x00` through
  compile-time knobs. This admits the charger/VBUS-safe interrupt subset only.
- MR, watchdog, rail, and fault interrupt sources stay disabled until each
  source has a dedicated target validation.
- ADP5360 interrupt flags are not cleared by reading. The power-owner snapshot
  must record the pre-clear flag bytes and then write `1` to the corresponding
  bits in `INTERRUPT_FLAG1` / `INTERRUPT_FLAG2` to clear them.
- `PMIC_INT` may request a power-owner PMIC snapshot. It must not directly
  trigger USB MSC export, storage handoff, installer entry, or any PMIC I2C
  operation from ISR context.

Rules:

- VBUS may wake the device, update charger policy, and notify USB policy.
- VBUS alone must not prompt for MSC flashing/export mode.
- USB protocol activity or successful host enumeration must gate MSC availability through [[Storage_and_Installer_Contract]].
- power-only chargers and USB-C power banks remain charger/external-power cases when no usable USB data-host activity is observed.

---

## System Power FSM

This state machine describes what the whole device is doing from a power-management perspective.

| State | Meaning |
|---|---|
| `PWR_BOOTING` | MCU has started, but clocks, rails, PMIC state, reset reason, and owner threads are not trusted yet. |
| `PWR_RAIL_VALIDATE` | Firmware validates power conditions, reset cause, required rails, and PMIC status before normal operation. |
| `PWR_ACTIVE_LP` | Awake low-power operation. Display/UI may update slowly, sensors are mostly off, and STOP residency is preferred. |
| `PWR_ACTIVE_RT` | Awake realtime operation. Used for gameplay, realtime display, audio, sensor streaming, install activity, or other high-duty work. |
| `PWR_SLEEP_PREP` | Owners quiesce DMA/peripherals, save required state, and arm approved wake sources. |
| `PWR_STOP_RESIDENT` | MCU is in STOP/low-power sleep. Display hold, RTC, and armed wake sources remain valid. |
| `PWR_WAKE_RESUME` | MCU woke and is restoring clocks, classifying wake reason, and resuming owners. |
| `PWR_FORCED_SLEEP` | Battery or policy requires sleep regardless of normal runtime intent. |
| `PWR_SHIP_PREP` | START hold indicates shipping mode may be reached soon; firmware saves and warns before hardware cutoff. |
| `PWR_FAULT` | Power state is unsafe or incoherent; boot/fault supervisor takes over. |

Rules:

- Only `thPower` transitions this FSM.
- STOP entry requires owner quiesce acknowledgements or explicit timeout/fault policy.
- Wake resume must classify wake source before handing control back to runtime policy.
- Forced sleep is the recoverable low-battery response.
- Controlled software shipment is the critical-battery response when enabled after save/quiesce; ADP5360 hardware BAT_UV / ISOFET protection remains the emergency fallback below firmware policy.

---

## ADP5360 / Battery FSM

This state machine describes what firmware knows about the PMIC, charger, and battery.

| State | Meaning |
|---|---|
| `PMIC_OFFLINE` | PMIC interface is not initialized or PMIC status is unavailable. |
| `PMIC_PROBE` | Firmware checks that the ADP5360 responds at I2C address `0x46`. |
| `PMIC_CONFIG` | Firmware applies required PMIC configuration and verifies it. |
| `PMIC_MONITOR` | Normal monitoring state for battery, charger, VBUS, interrupt, and fault status. |
| `PMIC_CHARGING` | Charger/input is present and battery is charging. Runtime may continue unless another mode blocks it. |
| `PMIC_CHARGE_DONE` | PMIC reports full or charge termination state. |
| `PMIC_LOW_BATT` | Battery crossed the low threshold; firmware warns, sheds optional load where possible, and may force recoverable sleep. |
| `PMIC_CRITICAL_BATT` | Battery crossed the critical threshold; firmware saves/quiesces what it can, then requests ADP5360 software shipment when the explicit gate is enabled. |
| `PMIC_SHIP_PENDING` | START hold indicates ADP5360 shipping-mode threshold may be reached soon. |
| `PMIC_RECOVERING` | Firmware is retrying or revalidating after a transient PMIC/status fault. |
| `PMIC_ERROR` | PMIC or battery state cannot be trusted. System must degrade or fault depending on severity. |

Rules:

- PMIC register access must be serialized by `thPower`.
- `PMIC_INT` may wake or notify, but handling occurs in `thPower` context.
- Charging state does not grant storage/installer ownership.
- Low battery, critical-battery shutdown, and START shipping intent are different policy paths and must stay separate.
- Critical battery uses controlled software shipment above the hardware BAT_UV fallback when the explicit Platform gate is enabled.
- Boot/restart policy must not allow battery rebound or START wake to loop back into runtime below the restart-allow threshold unless VBUS/charger recovery is present.

---

## Threshold Policy

Threshold values are tuning constants, not game policy.

Required thresholds:

- selected battery profile / cell family
- configured ADP5360 battery capacity value
- charger terminal voltage
- charger current
- charge termination current
- low battery warning threshold
- low battery forced-sleep threshold
- critical battery controlled-shipment threshold
- post-shipment restart-allow threshold
- critical-battery software-shipment enable gate
- charger-present debounce/filter timing
- VBUS disagreement timeout
- PMIC read retry limit

### HW6 Cell Profile And Validation State

This is the reviewed profile basis, not final production charge approval. On
HW6 unit 001, the six selected encodings passed a guarded no-cell/no-VBUS
write, exact readback, reverse restore, and exact restore readback. This proves
register acceptance. FW0 now also validates boot-applied conservative charger
profile write/readback, room-temperature `100 kOhm` NTC bias/readback, and
initial real-cell charging-state reporting at the retained low-current VBUS
baseline. JEITA zone behavior, protection thresholds, VBUS current policy,
charge termination, current promotion, and production charging behavior remain
gated by their dedicated tests.

| Item | Profile intent | ADP5360 candidate / present state |
|---|---|---|
| Cell | single-cell `303040` LiPo, `3.7 V` nominal, `4.20 V` terminal, `450 mAh` label | `BAT_CAP=0xE1` (`225 x 2 mAh`) accepted/read back/restored; gauge characterization remains open |
| Fast charge | maximum `320 mA` (`0.711 C`); dynamically limited by the validated VBUS source budget | conservative FW0 boot profile applies and reads back `0x04=0x29`; `0x04=0x3F` was accepted/read back/restored in the no-cell reversible test, but physical charging at that setting is not approved |
| Charge voltage | `4.20 V` | FW0 boot profile applies and reads back `0x03=0x82`; controlled terminal-voltage validation remains open |
| VBUS input limit | must follow the classified USB/source contract, independently of the cell charge-rate permission | FW0 boot profile applies and reads back `0x02=0x81` / `100 mA`; source-current policy and promotion remain open |
| Charger function / JEITA policy | enable the reviewed ADP5360 charger-function baseline without granting final JEITA behavior | FW0 boot profile applies and reads back `0x07=0xAC`; hot/cold/cool/warm substitution behavior remains open |
| Temperature | board-mounted `100 kOhm` NTC in physical contact with pouch; JEITA charging required | FW0 boot profile applies and reads back `0x0A=0x80`; real-cell/VBUS room-temperature THR status reports OK; hot/cold/cool/warm substitution behavior remains open |
| Fuel gauge | start in sleep mode and automatically enter active mode above its current threshold | `0x20=0xE1` and `0x27=0x53` accepted/read back/restored; FW0 prepare path validates nonzero VBAT/SOC reads; current-mode and capacity characterization remain open |
| Independent protection | ADP5360 protection is mandatory whether or not a development cell also contains a PCM | present `0x11..0x15 = 03 90 E6 78 E8`: enabled, `2.50 V` UV, `600 mA` discharge OC, `4.30 V` OV, `400 mA` charge OC; retain provisionally pending fault and peak-load tests |

The ADP5360 register limits and encodings above are derived from the
[ADP5360 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ADP5360.pdf).
The reversible HW6 result is recorded as
`EV-HW6-20260731-P1-ADP5360-003`.

Thresholds must be logged with firmware version during bring-up tests.

Rules:

- real-cell charging beyond the retained low-current baseline must not proceed until the selected cell chemistry, polarity, protection status, charge voltage, and charge-current limit are confirmed for that test.
- `1 C` cell permission does not grant `450 mA` charging: the ADP5360 maximum
  is `320 mA`, and the active VBUS/input contract may impose a lower limit.
- initial real-cell commissioning must begin at the already-observed `100 mA`
  setting; promotion toward `320 mA` requires current, temperature, VBUS-limit,
  charge-state, and fault evidence.
- ADP5360 protection configuration is required even when a development cell
  contains its own handling/assembly protection circuit.
- seller-stated pouch-cell capacity is not a design fact until measured or otherwise verified.
- ADP5360 fuel-gauge capacity configuration is a Platform battery-profile setting, not a package or game setting.
- if the physical cell capacity exceeds the ADP5360 fuel-gauge coding range, charger safety policy still follows the cell datasheet and PMIC limits; package-visible battery estimates must be treated as approximate until characterized.

---

## Failure Policy

PMIC/power faults are potentially fatal.

Fault handling depends on severity:

- transient I2C/status read failure: retry in `PMIC_RECOVERING`
- repeated PMIC read failure: degrade and report power-monitor fault
- low battery: warn, shed optional load, and force recoverable sleep if needed
- critical battery: save/quiesce where possible, then request gated software shipment
- hardware BAT_UV / ISOFET disconnect: emergency fallback below firmware policy
- incoherent VBUS/charger/install state: block installer ownership until resolved
- unsafe rail or reset condition: enter `PWR_FAULT`

---

## HW6 Validation Cases

1. ADP5360 probe at I2C address `0x46`
2. PMIC interrupt path from `PMIC_INT` on `PB15`
3. VBUS detected through ADP5360
4. VBUS detected through `USB_OTG_FS_VBUS` on `PA9`
5. VBUS path disagreement handling
6. PPK2 or equivalent battery-simulator operation across selected voltage points
7. ADP5360 battery-profile configuration for the selected cell
8. ADP5360 boot-applied conservative charger profile write/readback
9. VBUS-only charger/power-bank attach does not trigger MSC prompt or storage handoff
10. charging while normal runtime is active
11. charging while flashing/install mode is active
12. low-battery warning / recoverable forced sleep
13. critical-battery controlled software-shipment path, default-off gate, owner-ACK quiesce, and shared ADP5360 `0x36` software shipment primitive
14. post-shipment boot/restart gate below restart-allow threshold, including no-VBUS block, VBUS charge recovery, and UI recovery above restart-allow
15. hardware BAT_UV / ISOFET fallback validation
16. START hold shipping-prep handoff from input to power
17. ADP5360 `EN_MR_SD` enable during normal boot through `thPower`
18. START shutdown UI scaffold, owner-ACK quiesce barrier, release-cancel, and default-off software shipment request gate
19. pre-STOP sleep-prep scaffold with owner-ACK quiesce and active-LP recovery; real STOP2 entry intentionally skipped
20. manual STOP2 START-wake scaffold with owner-ACK quiesce, clock restore, active-LP recovery, and staged active-owner resume/liveness proof
21. PMIC_INT edge capture and `thPower` snapshot handling under a safe charger/battery/fault event

Evidence for these cases belongs in [[HW6_Brought_Up_Tracker]]. A passing HW5 result may define the initial procedure or expected value, but it does not close the corresponding HW6 row.
