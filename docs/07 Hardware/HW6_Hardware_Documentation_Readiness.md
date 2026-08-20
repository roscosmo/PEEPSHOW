# HW6 Hardware Documentation Readiness

This note tracks the documentation and configuration gates that must be
satisfied before HW6 CubeMX generation, first power, or subsystem promotion.

Status: `pre_arrival`

HW6 is the active hardware target. HW5 is retired and retained only as a
historical implementation and regression reference.

## Current Inputs

| Input | Status | Notes |
|---|---|---|
| Fabricated hardware design | frozen | schematic/PCB/BOM revision IDs still need to be recorded |
| HW6 full-intent IOC | imported | `firmware/peepshow_hw6_fw1/PEEPSHOW_HW6_FW1.ioc` |
| HW6 IOC SHA-256 | verified | `40801363273BB8ABD0072EFC5FFFF55E6878625B687033E03A6A5259E3DF179A` |
| HW6 safe arrival firmware target | not created | must be repository-contained and recoverable before first flash |
| Hardware delta | drafted | [[HW6_Delta_From_HW5]] |
| Arrival procedure | drafted | [[HW6_Arrival_Phase0_Checklist]] |
| Active tracker | drafted | [[HW6_Brought_Up_Tracker]] |

The imported `fw1` IOC is design input, not an approved first-power image.

## Readiness Gates

| Gate | Required Notes | Status | Blocking Detail |
|---|---|---|---|
| target identity | [[HW6_Hardware_Revision_Contract]] | partial | schematic, PCB, BOM, and assembly revision IDs pending |
| part identity | [[HW6_Part_Tokens]] | drafted | retained part numbers inherited from frozen design statement; verify against BOM release |
| hardware delta | [[HW6_Delta_From_HW5]] | drafted | three removals confirmed |
| pin authority | [[HW6_CubeMX_Pin_Map]], [[HW6_Pin_Ownership_Matrix]] | drafted | hardwired display translator and battery-connector `PWR_DBG` purpose recorded; generated defaults still require review |
| clocks | [[HW6_Clock_Tree_Contract]] | drafted | LPTIM1 source and SAI warning require generated-code review |
| DMA / memory | [[HW6_DMA_Map]] | drafted | linker placement and SRAM4 budgets require HW6 build/evidence |
| power / safe defaults | [[HW6_Power_Rails]] | drafted | translator is always enabled; verify `PWR_DBG` generated idle-low state and retained-rail behavior |
| wake sources | [[HW6_Wake_Sources]] | drafted | electrical behavior pending arrival proof |
| evidence partition | [[Evidence_Artifact_Convention]], [[HW6_Brought_Up_Tracker]] | drafted | first HW6 evidence artifact not yet created |
| revalidation scope | [[HW6_Revalidation_Matrix]] | drafted | execution pending hardware arrival |
| safe generated target | HW6 `fw0` or approved equivalent | blocked | path and generation strategy pending |

## Removed Capability Gates

The following HW5 work is closed as `not_applicable` for HW6:

- rotary-encoder electrical and wake validation
- TEMT6000 ambient-light calibration and ADC validation
- PAM8904/piezo/BBB electrical and audio validation

Their generic Engine capability contracts may remain for other targets and
host tooling, but the HW6 target profile must not grant them.

## Retained Runbook Coverage

| Subsystem | Reused Procedure | HW6 Requirement |
|---|---|---|
| PMIC / rails / battery | [[ADP5360_Power_Bring-up_Runbook]] | repeat all electrical and register evidence |
| display | [[LS013B7DH05_Display_Bring-up_Runbook]] | resolve OE, then repeat static, partial, DMA, and polarity proof |
| autonomous display | [[LPBAM_Autonomous_Display_Validation_Plan]] | repeat STOP2, wake/abort, memory, cadence, and handoff proof |
| external flash | [[AT25SL128A_External_Flash_Bring-up_Runbook]] | repeat non-destructive identity then bounded scratch tests |
| speaker | [[Audio_Output_Bring-up_Runbook]] | run speaker-only subset; BBB steps are not applicable |
| buttons | [[Button_Input_Bring-up_Runbook]] | repeat levels, EXTI, wake, debounce, interaction activation, and BOOT0 proof |
| joystick | [[TMAG3001_Joystick_Bring-up_Runbook]] | repeat identity, axes, threshold, interrupt, and current |
| IMU | [[LIS2DUX12_IMU_Bring-up_Runbook]] | repeat identity, lowest-power mode, events, and current |
| BLE/NFC | [[NINA_B112_BLE_Bring-up_Runbook]] | repeat UART, flow control, BLE, NFC, sleep, and current |
| USB/installer | [[USB_MSC_Bring-up_and_Recovery_Runbook]] | repeat attach, enumeration, ownership, recovery, and power |
| sleep/wake | [[Sleep_Wake_Integration_Bring-up_Runbook]] | use only HW6 wake sources and record HW6 current |

When an older runbook says HW5, its procedure may be reused only where the
[[HW6_Revalidation_Matrix]] admits it. All resulting evidence is HW6-specific.

## Pre-Generation Blockers

1. Record the fabrication schematic, PCB, BOM, and assembly revision IDs.
2. Verify generated firmware preserves the absence of `VLT_LCD`; the translator
   is hardwired enabled and must not be modeled as a software power control.
3. Verify `PWR_DBG` initializes low and remains package-inaccessible; any marker
   semantics belong to a target-qualified development evidence procedure.
4. Remove or verify stale CubeMX function-order references to ADC1 and TIM2.
5. Confirm `LPTIM1` remains internal for LPBAM and has no physical piezo output.
6. Review SAI1 generated clock registers against the intended 16 kHz stream.
7. Define the repository-contained safe HW6 arrival target and recovery method.
8. Confirm all unassigned removed-function pins remain safe and unowned.

## Arrival-Day Rule

Start with [[HW6_Arrival_Phase0_Checklist]]. Passing intake means only that a
specific HW6 board is safe enough for subsystem revalidation.

No capability becomes `granted` from visual similarity to HW5.
