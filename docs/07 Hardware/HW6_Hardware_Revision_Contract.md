# HW6 Hardware Revision Contract

HW6 is the active PeepShow hardware target and supersedes the retired HW5 target.

Status: `pre_arrival`

The board has been fabricated and is in delivery. Hardware behavior remains
`pending_validation` until measured HW6 evidence is linked from
[[HW6_Brought_Up_Tracker]].

## Design Basis

| Input | Authority | Status |
|---|---|---|
| Hardware target | `HW6` | frozen |
| MCU | `STM32U575RIT6`, LQFP64 | frozen |
| Full-intent CubeMX input | `firmware/peepshow_hw6_fw1/PEEPSHOW_HW6_FW1.ioc` | imported, not generated in this change |
| Full-intent IOC SHA-256 | `40801363273BB8ABD0072EFC5FFFF55E6878625B687033E03A6A5259E3DF179A` | verified after import |
| Schematic revision | fabrication release | `pending_record` |
| PCB revision | fabrication release | `pending_record` |
| BOM revision | fabrication release | `pending_record` |
| Assembly source/lot | delivery metadata | `pending_record` |
| Bring-up board IDs | assigned at intake | `pending_record` |

The schematic, PCB, BOM, and IOC revision identifiers must be recorded before
the first board is declared to match the expected revision.

## Revision Intent

HW6 retains the HW5 architecture with three physical capabilities removed:

- no `TEMT6000X01` ambient-light circuit
- no rotary encoder or encoder-enable circuit
- no `PAM8904EGPR` piezo/BBB path

The speaker, display, external flash, joystick, IMU, PMIC, BLE/NFC module,
buttons, USB, RTC, and STM32U575 remain.

The detailed delta is authoritative in [[HW6_Delta_From_HW5]].

## Hardware Authority Set

- [[HW6_Part_Tokens]]
- [[HW6_CubeMX_Pin_Map]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Clock_Tree_Contract]]
- [[HW6_DMA_Map]]
- [[HW6_Power_Rails]]
- [[HW6_Wake_Sources]]
- [[HW6_Hardware_Documentation_Readiness]]
- [[HW6_Revalidation_Matrix]]

## Target Capability Consequences

The HW6 target profile must publish:

- `input.encoder_supported = false` and capability `input.encoder` blocked
- `sensors.light_supported = false`
- `audio.bbb_supported = false`

Packages may use those capabilities only when another selected target grants
them and package compatibility rules are satisfied. Their historical HW5
contracts do not grant them on HW6.

## CubeMX Reconciliation Gates

The imported IOC confirms the three removed paths and agrees with these
resolved HW6 design decisions:

1. The display level translator is hardwired enabled so EXTCOMIN can always
   reach the powered panel. HW6 therefore has no `PD2` / `VLT_LCD` GPIO and no
   software translator-disable policy.
2. `PH1` / `PWR_DBG` is a spare output routed to the battery connector for a
   PPK2 logic input. Platform diagnostic builds may use it as an idle-low
   RTOS, timing, or power-correlation marker. Marker polarity and meaning are
   defined by each evidence procedure; packages cannot access it.

The IOC also retains stale `ProjectManager.functionlistsort` references to
`MX_TIM2_Init` and `MX_ADC1_Init` even though `TIM2`, `ADC1`, and their physical
pins are absent from the HW6 MCU/IP assignment. Regeneration must prove that
removed initializers are not emitted.

`LPTIM1` is intentionally retained without a physical `PB2` output because it
serves autonomous-display/LPBAM timing. It must not be removed with the piezo
path.

## Known HW6 Unit 001 Electrical Findings

- `USB_OTG_FS_VBUS` / `PA9` is not reliable as a digital VBUS-present source on
  the measured HW6 unit 001 assembly. The board divider was confirmed as
  `5 V -> 47 kOhm -> PA9 -> 15 kOhm -> GND`, which produces approximately
  `1.2 V` from a `5 V` USB input. That is below a safe STM32U575 `3.3 V` GPIO
  high threshold. Firmware must treat PA9 VBUS as diagnostic-only on this
  revision and use ADP5360 VBUS plus USB protocol evidence for policy decisions.

## Evidence Boundary

- HW5 evidence is historical regression context only.
- No HW5 electrical, timing, current, wake, or target-profile result is
  automatically valid for HW6.
- Every HW6 artifact identifies target, board ID, board revision, assembly
  source/lot, firmware commit, and hardware rework state.
- Removed HW5 capabilities are not tested as present hardware on HW6.

## Sign-Off Checklist

The HW6 hardware contract is ready for generated firmware only when:

1. schematic, PCB, BOM, and assembly revision IDs are recorded
2. [[HW6_Delta_From_HW5]] agrees with fabrication data
3. the IOC is checked against [[HW6_CubeMX_Pin_Map]]
4. the hardwired display translator and dev-only `PWR_DBG` policy are preserved
5. removed peripheral initializers are absent after generation
6. pin ownership, clocks, DMA, power rails, and wake sources agree
7. the safe arrival firmware target and recovery path are identified
