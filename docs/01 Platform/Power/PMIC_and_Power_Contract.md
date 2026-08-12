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

The current FW0 START scaffold routes input-owned hold events to `thPower`, records them in the power state-machine probe, preserves the prior active return state, enters existing `PWR_SHIP_PREP` / `PMIC_SHIP_PENDING` states, asks every physical owner to quiesce through its own ThreadX queue with bounded ACK waits, and asks `thUI` to show a plain `SHUTDOWN` scaffold page. Release before shipment requests cancel and returns the UI to the prior page. FW0 also contains a guarded power-owner primitive for ADP5360 software shipment entry by writing Shipment Mode register `0x36 = 1`. START imminent may request that primitive only when `KNOB_POWER_START_SOFTWARE_SHIP_ENABLE` is true; the generated default is false, so normal bring-up START tests do not software-enter shipment. HW6 unit 001 validation confirmed the owner quiesce barrier on START prep with required/send/ACK/success/failure masks `0x7e/0x7e/0x7e/0x7e/0x0`, all owner action statuses `0x0`, and power/PMIC held in `PWR_SHIP_PREP` / `PMIC_SHIP_PENDING` (`8/8`). Earlier validation confirmed the default-off START software shipment gate stayed closed during a full prep/warning/imminent/release sequence: software shipment enable/request/skip was `0/0/1`, PMIC software shipment request count stayed `0`, and the UI/display returned to HOME after release.

HW6 unit 001 evidence `EV-HW6-20260811-P1-SHIP-032` validates the underlying software shipment primitive itself. A debugger-only manual request set `g_ps_hw6_pmic_software_ship_request = 1` while the device was in normal active power state `2 / 3`; after continue, the target lost power/connection with `Remote failure reply: E31`, and the device restarted only after a START press. This proves the firmware-owned ADP5360 `0x36 = 1` path can enter shipment mode without waiting for the 12-second hardware MR threshold. It does not validate automatic START-imminent, critical-battery, or boot-low-battery software shipment gates.

This path is a normal boot power-owner action. It is not part of the retained
peripheral diagnostic lifecycle and must not require display, audio, sensor,
storage, or communication diagnostic cycles.

---

## Critical-Battery Shipment And Restart Gate

Critical-battery shutdown is a PeepOS-controlled sequence while firmware is still alive enough to make a safe decision.

Rules:

- On critical battery, `thPower` must request owner quiesce/save through bounded Platform-owned hooks before any software shipment request.
- If the critical-battery software-shipment gate is disabled, firmware must record and expose that shipment would have been requested, but it must not write ADP5360 Shipment Mode register `0x36`.
- If the gate is enabled and quiesce/save policy succeeds or reaches its bounded fallback, `thPower` may request ADP5360 Shipment Mode register `0x36 = 1`.
- The ADP5360 hardware BAT_UV / ISOFET cutoff remains a lower emergency protection path if firmware cannot act in time.
- Startup must read battery/VBUS state before enabling display-intensive work, audio, vibration, radio, switched rails, package runtime, or installer behavior.
- Battery threshold decisions must use a valid decoded VBAT measurement; successful I2C reads with all-zero/raw-invalid fuel-gauge voltage must be treated as unknown and must not trigger critical shutdown.
- ADP5360 fuel-gauge VBAT reads must not be trusted until `thPower` has configured fuel-gauge active mode and requested an SOC/VBAT refresh.
- If VBUS/charger recovery is present, startup may remain in a charge/recovery shell even below the restart-allow threshold.
- If VBUS is absent and battery is below the restart-allow threshold after a shipment wake or low-battery reset, firmware must not continue normal runtime. With the boot-low-battery shipment gate disabled, it records a would-ship result; with the gate enabled, it re-enters software shipment.
- The boot/restart battery check remains pending until the first valid fuel-gauge VBAT sample is available. It must not treat the initial unavailable fuel reading as permission to enter normal runtime.
- While boot/restart is blocked without VBUS, the UI may show a plain low-battery recovery page. With VBUS present and VBAT below the restart-allow threshold, the UI may show a plain charging recovery page. Neither page is a normal shutdown-cancel target; both clear only when `thPower` reports VBAT at/above the restart-allow threshold.
- The restart-allow threshold must be higher than the critical-shutdown threshold.

Provisional FW0 thresholds, pending HW6 measurement and UX review:

| Threshold / gate | Provisional value | Purpose |
|---|---:|---|
| battery monitor cadence | `1000 ms` | periodic `thPower` PMIC snapshot while FW0 monitor scaffold is active |
| warning threshold | `3500 mV` | show warning and reduce optional load |
| critical controlled-shipment threshold | `3300 mV` | save/quiesce then request software shipment when enabled |
| restart-allow threshold | `3600 mV` | prevent battery rebound from restarting normal runtime |
| critical software-shipment gate | `false` | keep bring-up tests from powering off unexpectedly |
| boot-low-battery shipment gate | `false` | record would-ship until the boot path is validated |

HW6 FW0 target evidence now validates the runtime policy path at one normal point, one warning point, one critical point, one runtime recovery point, the no-VBUS boot/restart block, the VBUS-present boot charge-recovery path, the START owner-ACK quiesce barrier, the pre-STOP sleep-prep owner-ACK scaffold, the manual STOP2 START-wake scaffold, and the shared ADP5360 software shipment primitive. The no-VBUS boot-gate case used a controlled source: `3270 mV` held policy state `BOOT_RESTART_BLOCKED`, kept power/PMIC in `PWR_SHIP_PREP` / `PMIC_SHIP_PENDING`, showed UI/display shutdown state `LOW_BATT_BOOT`, and recorded a default-off software-shipment skip. Raising the source produced `3707 mV`, cleared the boot gate, returned power/PMIC to `PWR_ACTIVE_LP` / `PMIC_MONITOR`, and returned UI/display to HOME. The VBUS-present case used the restart threshold forced to `4200 mV`: PMIC-read `3968 mV` selected `BOOT_CHARGE_RECOVERY`, suppressed HOME, showed UI/display shutdown state `LOW_BATT_CHARGE`, kept shipment requests at zero, and PMIC entered `PMIC_CHARGING`. Restoring the restart threshold to `3600 mV` with PMIC-read `4049 mV` cleared the boot gate and returned UI/display to HOME while PMIC stayed charging. `EV-HW6-20260811-P1-SHIP-032` then proved a manual power-owner request can write ADP5360 `0x36 = 1` and place the device in shipment mode. Threshold values remain provisional until UX, current, automatic gated software-shipment paths, and persistent save evidence are complete. `EV-HW6-20260811-P1-SLEEP-034` proves sleep-prep can enter `PWR_SLEEP_PREP`, collect owner ACKs with masks `0x7e/0x7e/0x7e/0x7e/0x0`, intentionally skip STOP entry, and recover to `PWR_ACTIVE_LP`. `EV-HW6-20260811-P1-STOP2-035` proves the follow-on manual path can enter real STOP2 after the same owner-ACK barrier, wake from START on PA4, restore clocks, recover the power FSM to `PWR_ACTIVE_LP`, collect baseline post-wake owner ACK/liveness proof with inactive/parked owners, and complete a staged active-owner STOP2 pass where audio/input/display/sensor/comm are active, quiesced, STOP2-entered, then resumed or confirmed live after wake. Storage/flash were intentionally excluded from the staged active-owner pass to avoid repeating flash scratch/erase/write tests; production automatic STOP admission, wake classification, tick compensation, LPBAM, current, repeated cycles, and fault-injection remain open.

---

## VBUS Detection

VBUS may be classified from:

- ADP5360 charger/input status
- MCU `USB_OTG_FS_VBUS` on `PA9`

`thPower` records both paths for diagnostics, but on current HW6 unit 001 policy must prefer the ADP5360 VBUS view because the PA9 divider output is below the reliable digital-high range.

Disagreement between the PMIC VBUS view and MCU VBUS view is a diagnostic event until explained. It must not silently change installer/storage ownership.

VBUS classification means external USB power is present. It does not by itself prove a USB data host exists.

USB data-host classification is not derived from VBUS. FW0 diagnostics must report PMIC VBUS, MCU `PA9` VBUS, PMIC/MCU agreement, and USB protocol proof separately so charger-only attach cannot be confused with host-driven installer/export eligibility.

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
