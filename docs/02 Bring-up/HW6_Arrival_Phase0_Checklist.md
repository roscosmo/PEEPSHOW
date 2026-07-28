# HW6 Arrival and Phase 0 Checklist

This is the first procedure for each initial HW6 board. It is an intake and
safety gate, not subsystem validation.

Status: `drafted`

Execution status: `blocked` until the remaining pre-arrival documentation gates and safe HW6 arrival firmware target are closed.

Related:

- [[HW6_Hardware_Revision_Contract]]
- [[HW6_Delta_From_HW5]]
- [[HW6_Hardware_Documentation_Readiness]]
- [[HW6_Brought_Up_Tracker]]
- [[HW6_Revalidation_Matrix]]
- [[Evidence_Artifact_Convention]]
- [[HW6_Power_Rails]]
- [[HW6_CubeMX_Pin_Map]]

## Pre-Arrival Preparation

Before applying power:

1. Record schematic, PCB, BOM, and assembly release identifiers.
2. Import and hash the exact IOC used as firmware design input.
3. Confirm the fabrication release preserves the hardwired-enabled display
   translator and battery-connector `PWR_DBG` route documented for HW6.
4. Assign a stable board ID to each received unit.
5. Prepare an HW6 evidence folder and manifest.
6. Prepare current-limited power, PPK2/bench measurement, DMM, magnification,
   camera, and SWD recovery equipment.
7. Identify test points for input, 1.8 V, 3.3 V, reset, display, and ground.
8. Do not prepare a normal application flash until safe power and recovery are
   proven.

## Evidence Folder

Use:

```text
docs/02 Bring-up/Evidence/HW6/YYYY/MM/DD/EV-HW6-YYYYMMDD-P0-ARRIVAL-NNN/
```

Minimum artifacts:

- top and bottom board photos
- board marking and assembly label photos
- resistance/continuity notes
- first-power current capture
- critical rail measurements
- first debugger attach/recovery log
- deviations, rework, or suspect assembly notes

## Intake Metadata

| Field | Value |
|---|---|
| Date/time |  |
| Hardware target | `HW6` |
| Board ID / serial |  |
| PCB revision |  |
| Schematic revision |  |
| BOM revision |  |
| Assembly source / lot |  |
| Hardware rework state | `none` or explicit list |
| Firmware commit / image |  |
| IOC hash |  |
| Instruments and versions |  |
| Evidence ID |  |

## Visual Inspection

With all power removed:

1. Photograph both sides before rework.
2. Confirm revision markings and assembly identity.
3. Verify MCU, PMIC, display translator, flash, IMU, joystick, NINA, speaker
   amp, connectors, and polarized components.
4. Confirm the TEMT6000 circuit is not populated.
5. Confirm the rotary encoder and encoder support circuit are not populated.
6. Confirm the PAM8904/piezo path is not populated.
7. Inspect PMIC and BGA/LGA footprints for bridges, void symptoms, or skew.
8. Inspect USB, battery, display, speaker, and SWD connections.
9. Inspect the hardwired display-translator enable and `PWR_DBG`
   battery-connector route against fabrication data.
10. Stop for any defect that could short a rail, reverse polarity, hold reset,
    drive a retained peripheral unsafely, or invalidate the expected revision.

## Unpowered Electrical Gate

Record measured results:

| Check | Acceptance |
|---|---|
| main input to GND | no unexplained hard short |
| battery path to GND | no unexplained hard short |
| 1.8 V rail to GND | consistent with design; no unexplained hard short |
| 3.3 V rail to GND | consistent with design; no unexplained hard short |
| MCU VDD to GND | no unexplained hard short |
| display supply and translator path | no unexplained hard short |
| flash supply | no unexplained hard short |
| speaker amp supply | no unexplained hard short |
| USB VBUS to GND | no unexplained hard short |
| SWDIO/SWCLK continuity | present |
| reset / BOOT0 / Start-MR paths | no obvious short or wrong strap |
| removed light/encoder/PAM nets | no accidental population or backfeed path |

Do not infer a pass solely by comparing resistance with one HW5 board. Record
instrument behavior and settling/drift where relevant.

## First-Power Gate

1. Keep the battery disconnected unless the controlled setup requires it.
2. Use a current-limited source with immediate disconnect available.
3. Start at a conservative limit chosen from the design and instrument setup;
   record it rather than freezing a guessed value here.
4. Observe input current, 1.8 V, 3.3 V, reset, heat, odor, and PMIC behavior.
5. Do not initialize display transfers, flash writes, audio, BLE, USB MSC,
   STOP2, LPBAM, or normal package runtime.
6. Remove power immediately for unexpected current, collapsed rails, heating,
   unstable reset, or PMIC fault.

| Observation | Expected Result | Measured | Status |
|---|---|---|---|
| source voltage/current limit | recorded |  | open |
| initial current | no short-current behavior |  | open |
| 1.8 V rail | stable in expected design range |  | open |
| 3.3 V rail | stable in expected design range |  | open |
| reset / BOOT0 / Start | sane idle/release behavior |  | open |
| display translator / EXTCOMIN path | hardwired enabled; EXTCOMIN reaches panel |  | open |
| `PWR_DBG` | low/idle unless an explicit diagnostic marker test is active |  | open |
| NINA reset/auxiliary pins | reset/high-Z policy |  | open |
| speaker shutdown | disabled |  | open |
| PMIC interrupt/fault | explainable |  | open |
| visible heating | none unexpected |  | open |

## First Debugger And Recovery Gate

After safe power is established:

1. Attach over SWD without flashing.
2. Read and record MCU identity.
3. Prove halt and reset-under-debug.
4. Prove attach-under-reset recovery.
5. Read reset-cause and key status without driving unresolved GPIO.
6. Flash only an approved HW6 safe-arrival image.
7. Record the exact ELF hash, commit, tool invocation, and target voltage.
8. Confirm the board remains recoverable after reset.

Do not flash an HW5 image onto HW6.

## Exit Criteria

Phase 0 passes only when:

- board and fabrication identities are recorded
- inspection and unpowered checks have no unresolved safety blocker
- first-power current and critical rails are captured
- hardwired translator behavior is confirmed and `PWR_DBG` is idle-low
- debugger recovery is proven
- an HW6 evidence manifest exists
- [[HW6_Brought_Up_Tracker]] is updated

Passing means only:

```text
This HW6 unit is safe enough to begin bounded subsystem revalidation.
```
