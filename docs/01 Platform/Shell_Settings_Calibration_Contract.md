# Shell, Settings, and Calibration Contract

This document defines the Platform-owned shell, system settings, first-setup, calibration, and package-management UX boundary.

The shell must remain useful without the Reference Game installed.

Related:

- [[Shell_and_UI_Navigation_State_Machine]]
- [[Platform_Freeze_Charter]]
- [[Package_Contract]]
- [[Package_Save_Settings_API_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Input_Focus_API_Contract]]
- [[Sensor_API_Contract]]
- [[Storage_and_Installer_Contract]]
- [[USB_Development_Mode_Contract]]
- [[Brought_Up_Tracker]]

---

## Purpose

The Platform shell owns:

- first setup
- local date/time setup
- system settings
- package browser and launch
- package install/remove entry points
- input calibration
- sensor calibration where needed
- diagnostic summaries
- safe error/recovery flows

The shell is Platform UI. It is not Reference Game UI.

---

## Boundary

Shell/system settings may control:

- PeepOS local date/time
- global sound/mute policy
- display/system UI preferences
- input calibration and repeat/accessibility policy
- package management
- diagnostics/export entry points
- installer/developer USB personality entry where policy allows

Shell/system settings must not expose:

- Platform knobs as normal user settings
- HAL, pins, buses, DMA, clocks, RTOS, middleware, or memory addresses
- raw sensor registers or calibration storage
- BLE bonding internals
- flash regions or filesystem paths
- Reference Game mechanics

Package settings are separate and are governed by [[Package_Save_Settings_API_Contract]].

---

## First Setup Flow

Required first-setup states:

```text
FIRST_SETUP_START
FIRST_SETUP_SET_DATE_TIME
FIRST_SETUP_INPUT_CHECK
FIRST_SETUP_DISPLAY_CHECK
FIRST_SETUP_SOUND_POLICY
FIRST_SETUP_PACKAGE_PROMPT
FIRST_SETUP_COMPLETE
```

Rules:

- local calendar time is set by PeepOS shell/setup only.
- packages may read valid local date/time where target profile grants it.
- packages may not set, correct, resync, or directly program RTC/calendar time.
- setup must be recoverable after interrupted power.
- setup completion state is a Platform setting, not a package setting.

---

## System Settings Domains

Initial system settings domains:

| Domain | Examples | Owner |
|---|---|---|
| `time` | local date, local time, date format where supported | Platform time/shell |
| `sound` | global mute, system volume policy where supported | Platform audio/shell |
| `display` | system UI contrast/theme policy where supported | Platform display/shell |
| `input` | repeat behavior, input accessibility, calibration route | Platform input/shell |
| `power` | user-visible sleep/shipping actions only | Platform power/shell |
| `packages` | installed package list, launch/default package | package manager/shell |
| `diagnostics` | export logs, view fault summary, device info | fault supervisor/shell |
| `usb` | installer/export mode, developer mode entry where allowed | Platform storage/USB |

Rules:

- user settings are not Platform knobs.
- settings must use versioned records and safe persistence through Platform storage.
- changing settings must not bypass owner requests.
- developer-only controls must be hidden outside dev-mode policy.
- package-owned settings may be displayed inside a package settings panel but remain package-owned records.

---

## Calibration Domains

Calibration is Platform-owned.

Initial calibration domains:

| Domain | Purpose | Package Access |
|---|---|---|
| buttons | verify logical action mapping and stuck input behavior | normalized actions only |
| encoder | direction, detent ratio, repeat/acceleration policy where supported | normalized deltas only |
| joystick | center, deadzone, direction/vector mapping | normalized vector/direction only |
| light sensor | ambient bands and filtering policy | resolved value/band only |
| IMU | orientation/motion normalization where required | normalized events/snapshots only |
| display | visible pattern tests and system UI checks | no direct access |
| audio | speaker/BBB audible check and global mute policy | symbolic audio APIs only |

Rules:

- calibration records are not package settings.
- packages cannot read or write raw calibration records.
- calibration changes must be owner-routed and persisted safely.
- calibration evidence from bring-up must be separated from user calibration records.
- hardware-dependent calibration flows may remain drafted until measured behavior exists for the active target; HW6 evidence is recorded in [[HW6_Brought_Up_Tracker]].

HW6 joystick calibration currently uses the shell sequence `CENTER -> UP ->
RIGHT -> DOWN -> LEFT -> FULL SWEEP -> REVIEW`. `thInput` performs each bounded
capture incrementally and publishes fixed-point progress; the shell renders the
progress without reading the sensor. Calibration and review veto STOP2. The
review page provides a live marker against the measured aligned envelope,
deadzone, activation-threshold markers, and current dominant cardinal result.
A starts a bounded owner-routed save while REVIEW displays `SAVING`; the
candidate becomes active only after `thStorage` verifies the record body,
writes the commit marker last, and rescans the protected calibration region.
A successful save exits calibration to HOME. A failed save remains on REVIEW with
`TRY AGAIN`. B restores the previous
calibration before a save starts. At boot, normal joystick routing remains
disabled until the protected records are scanned and applied. Missing,
conflicting, or invalid records route directly to the button-navigable joystick
calibration flow rather than starting normal shell/game joystick input.

---

## Package Browser And Management

The shell package browser owns:

- installed package list
- package metadata display
- compatibility report display
- package launch
- package settings entry where supported
- package reset/uninstall prompts where policy allows
- installer/export handoff

Rules:

- package launch uses [[Runtime_Host_Contract]].
- package compatibility uses [[Target_Profile_Schema_Contract]].
- package settings use [[Package_Save_Settings_API_Contract]].
- install/remove/reset operations route through Platform storage/package manager owners.
- shell must remain functional if a package fails validation, launch, or runtime execution.

---

## Diagnostics And Recovery

Shell diagnostics should expose bounded summaries:

- firmware version/build profile
- board revision where known
- target profile
- last reset reason
- fault summary
- package fault summary
- storage/install status
- battery/power status
- input/sensor/audio/display health summary
- export logs/artifacts entry point

Rules:

- diagnostics must not expose protected raw storage or arbitrary filesystem paths.
- user-facing diagnostics may summarize Platform faults, but detailed evidence export remains controlled.
- recovery flows must route to shell-safe states and must not require Reference Game code.

---

## Validation Cases

1. shell boots and reaches home with no package installed.
2. first setup records valid local date/time through Platform flow.
3. package code cannot set RTC/calendar time.
4. global mute/system sound setting affects package audio output through Platform policy.
5. package-owned setting edits route through package settings API.
6. calibration records are not visible through package APIs.
7. package launch failure returns to shell with diagnostic summary.
8. installer/export entry preserves storage ownership rules.
9. shell diagnostics export bounded artifacts only.
10. shell remains usable after package runtime fault.

---

## Rule

The shell is the PeepOS console UI.

It owns system setup, settings, calibration, package management, and recovery without relying on the Reference Game.
