# HW5 Arrival and Phase 0 Checklist

This checklist was the first procedure used when HW5 hardware arrived.

> [!warning] Retired checklist
> HW5 is retired. This checklist remains historical procedure only and must not be used for an HW6 unit. Use [[HW6_Arrival_Phase0_Checklist]].

It is an intake and safety gate. It does not prove any subsystem known-good.

Subsystem behavior becomes known-good only through the relevant runbook and evidence linked from [[Brought_Up_Tracker]].

Related:

- [[Brought_Up_Tracker]]
- [[Evidence_Artifact_Convention]]
- [[Pending_Measured_Constants_Register]]
- [[FW0_Phased_CubeMX_Bring-up_Plan]]
- [[ADP5360_Power_Bring-up_Runbook]]
- [[HW5_Hardware_Documentation_Readiness]]
- [[HW5_Power_Rails]]
- [[HW5_CubeMX_Pin_Map]]
- [[HW5_Pin_Ownership_Matrix]]
- [[HW5_Wake_Sources]]

---

## Purpose

Before normal bring-up begins, prove:

- the received board matches the expected HW5 revision
- the board has no obvious mechanical, assembly, or solder fault
- power can be applied safely under current-limited observation
- critical rails and reset/default states are sane enough to continue
- a debugger can connect without changing hardware policy
- evidence folders, tracker metadata, and blocking issues are initialized

This checklist exists so the first hardware session is procedural, recorded, and reversible.

---

## Scope

This checklist covers:

- board receipt
- visual inspection
- unpowered electrical checks
- safe first power
- first rail and reset observations
- first debugger connection
- evidence and tracker setup

This checklist does not cover:

- PMIC threshold policy
- battery charge/discharge validation
- clock-tree validation
- display operation
- storage erase/program/readback
- audio playback
- input normalization
- sleep/wake behavior
- USB MSC/CDC behavior
- BLE/NINA behavior
- digital twin work

Those are covered by subsystem runbooks.

---

## Required Equipment

Prepare:

- schematic and PCB viewer/render for the received revision
- current-limited bench supply or protected power input path
- USB power meter where USB is used
- multimeter with continuity/resistance mode
- oscilloscope or logic analyzer where available
- SWD debugger
- known-good USB cable
- magnification for visual inspection
- ESD-safe work area
- camera for board photos
- battery simulator or controlled battery path if battery tests are attempted later

Do not use a real battery as the first unknown power source unless the board design and recovery path require it and the risk has been explicitly accepted.

---

## Pre-Arrival Preparation

Before the board is powered:

1. Confirm the active bring-up `.ioc` path from [[FW0_Phased_CubeMX_Bring-up_Plan]].
2. Confirm the expected board revision and schematic revision.
3. Confirm the expected safe GPIO defaults for display level translator, NINA auxiliary pins, PMIC/reset-related pins, and storage/USB ownership.
4. Prepare the evidence folder path using [[Evidence_Artifact_Convention]].
5. Prepare an initial tracker row in [[Brought_Up_Tracker]].
6. Prepare a list of known high-risk nets to inspect before power:
   - main power input
   - battery path
   - regulated MCU/system rail
   - PMIC interrupt and MR/Start path
   - display level-translator enable
   - NINA reset and auxiliary pins
   - external flash supply and chip select
   - speaker amplifier enable
   - SWD pins
7. Prepare a minimal firmware/debug plan, but do not flash until safe power and debugger connection are proven.

---

## Evidence Folder Setup

Create one evidence folder for the intake session:

```text
docs/02 Bring-up/Evidence/YYYY/MM/DD/EV-YYYYMMDD-P0-ARRIVAL-001/
```

Create `manifest.md` using the template from [[Evidence_Artifact_Convention]].

Minimum intake artifacts:

- top-side board photo
- bottom-side board photo
- close-up photos of any suspect assembly area
- unpowered resistance/continuity notes
- first-power current and rail notes
- first-debugger connection log or screenshot
- notes on deviations from expected revision/configuration

Do not store host-private paths or secrets in the evidence folder.

---

## Intake Metadata

Record:

| Field | Value |
|---|---|
| Date/time |  |
| Board serial/label |  |
| Board revision |  |
| Assembly revision/source |  |
| Schematic revision used for inspection |  |
| Firmware commit initially used |  |
| Debugger/tool versions |  |
| Maintainer |  |
| Evidence ID |  |

Update the metadata block in [[Brought_Up_Tracker]] once the board revision and firmware commit are known.

---

## Visual Inspection

With no power applied:

1. Photograph the board top and bottom.
2. Confirm board revision markings match the expected revision.
3. Inspect all IC orientations.
4. Inspect battery connector polarity and mechanical clearance.
5. Inspect USB connector soldering and mechanical stability.
6. Inspect display connector, level translator area, and display power path.
7. Inspect PMIC, MCU, external flash, IMU, BLE module, audio amp, and sensor footprints.
8. Inspect SWD connector/pads.
9. Look for solder bridges, missing components, tombstoned passives, damaged parts, foreign material, or visible shorts.
10. Compare any assembly deviation against schematic/PCB expectations before power.

Stop if any defect could plausibly short power, reverse polarity, hold reset incorrectly, or drive an external module into an unsafe state.

Record every stop condition in [[Brought_Up_Tracker]] as an open issue.

---

## Unpowered Electrical Checks

With no battery, USB, debugger power, or bench supply attached unless the measurement requires it:

| Check | Expected Result | Measured Result | Status |
|---|---|---|---|
| main input to GND resistance | no hard short |  | open |
| battery path to GND resistance | no hard short |  | open |
| regulated system rail to GND resistance | no hard short |  | open |
| MCU VDD rail to GND resistance | no hard short |  | open |
| display supply path to GND resistance | no hard short |  | open |
| external flash supply to GND resistance | no hard short |  | open |
| audio amp supply to GND resistance | no hard short |  | open |
| USB VBUS to GND resistance | no hard short |  | open |
| SWDIO/SWCLK continuity to connector/pads | continuity present |  | open |
| reset line level/resistance sanity | no obvious short |  | open |
| BOOT0 / dev boot input sanity | no obvious short |  | open |
| Start/MR path sanity | no obvious short |  | open |

Use the schematic and board design to decide whether any low resistance is expected. If unsure, stop and inspect before power.

Do not continue to first power with an unexplained hard short.

---

## First-Power Safety Gate

Before applying power:

1. Keep battery disconnected unless the test plan explicitly requires it.
2. Use a current-limited source or protected USB power path.
3. Start with the most conservative current limit that can reveal a short without damaging the board.
4. Be ready to disconnect power immediately.
5. Keep fingers clear of conductive areas.
6. Do not connect display, speaker, battery, or external accessories unless required for the specific first-power setup.
7. Do not run START shipping-mode, low-battery, critical-battery, storage write, display transfer, audio output, sleep, or BLE tests during this intake step.

If current draw jumps unexpectedly, a rail collapses, a part heats quickly, or smoke/odor appears, remove power and record a blocking issue.

---

## First-Power Observations

Apply power through the chosen safe path and record:

| Observation | Expected Result | Measured Result | Status |
|---|---|---|---|
| input current at initial power | no short-current behavior |  | open |
| main input voltage | stable |  | open |
| regulated system rail | stable and in expected range |  | open |
| MCU VDD rail | stable and in expected range |  | open |
| reset line | sane reset/release behavior |  | open |
| BOOT0/dev input | expected idle state |  | open |
| Start/MR path | expected idle state |  | open |
| display level-translator enable | safe/disabled default before display owner |  | open |
| NINA reset/auxiliary pins | safe default before comm owner |  | open |
| external flash CS/reset/hold pins | safe idle default |  | open |
| speaker amplifier enable | safe/off default |  | open |
| visible heat | none unexpected |  | open |

Record the power source and current limit used.

If the observed default state contradicts [[HW5_CubeMX_Pin_Map]], [[HW5_Pin_Ownership_Matrix]], or a Platform contract, record the contradiction before changing firmware.

---

## First Debugger Connection

After safe power is observed:

1. Connect SWD without changing power assumptions.
2. Attempt connection under reset if normal attach fails.
3. Read MCU identity.
4. Confirm reset/halt/resume works.
5. Record debugger, probe, command, and result.
6. Do not flash application firmware until the board can be recovered.
7. If flashing a minimal smoke-test image, record the exact firmware commit and build profile.

Minimum evidence:

| Check | Expected Result | Measured Result | Status |
|---|---|---|---|
| SWD attach | debugger connects |  | open |
| MCU ID read | expected STM32 identity |  | open |
| halt/resume | stable |  | open |
| reset under debugger | stable |  | open |
| optional minimal firmware load | succeeds and is recoverable |  | open |

Do not use debugger memory pokes as a substitute for Platform owner behavior once owner firmware exists.

---

## Phase 0 Exit Criteria

Phase 0 intake is complete only when:

- board revision is recorded
- visual inspection has no unresolved power-risk defects
- unpowered checks show no unexplained hard shorts
- safe first power is recorded
- critical rails and reset/default states are sane enough to continue
- debugger connection is proven or a blocking issue is recorded
- evidence folder exists with manifest and initial artifacts
- [[Brought_Up_Tracker]] metadata and evidence ledger are updated
- any deviations are listed as open issues or temporary measures

Passing this checklist means only:

```text
HW5 board is safe enough to begin subsystem bring-up.
```

It does not mean PMIC, clocks, display, storage, audio, sensors, BLE, sleep, package runtime, or digital twin behavior is known-good.

---

## Hand-Off To Subsystem Bring-up

Recommended order after this checklist:

1. [[ADP5360_Power_Bring-up_Runbook]]
2. basic clock/reset stability from [[HW5_Clock_Tree_Contract]]
3. [[LS013B7DH05_Display_Bring-up_Runbook]]
4. [[AT25SL128A_External_Flash_Bring-up_Runbook]]
5. remaining subsystem runbooks according to [[Brought_Up_Tracker]]

Do not skip Phase 0 evidence when a later subsystem appears to work.

---

## Tracker Row Template

Add a row like this to [[Brought_Up_Tracker]]:

| Date | Test Case | Mode/Host | Result | Artifact | Notes |
|---|---|---|---|---|---|
| YYYY-MM-DD | P0-ARRIVAL-INTAKE | bench intake | PASS/FAIL | `docs/02 Bring-up/Evidence/YYYY/MM/DD/EV-YYYYMMDD-P0-ARRIVAL-001/` | board revision, power source, blockers |

---

## Rule

The first board session is for safety, identification, and evidence setup.

Do not use it to declare subsystem behavior known-good.
