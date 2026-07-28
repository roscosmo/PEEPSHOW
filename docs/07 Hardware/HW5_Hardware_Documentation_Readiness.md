# HW5 Hardware Documentation Readiness

This note tracks what must be documented before CubeMX generation and firmware implementation are allowed to shape architecture.

> [!warning] Archived readiness record
> HW5 is retired. This note records the old readiness process and must not gate or authorize HW6 work. Use [[HW6_Hardware_Documentation_Readiness]].

Hardware documentation is considered ready when the board authority docs, platform contracts, and bring-up runbooks agree with each other.

Hardware-dependent constants, timings, thresholds, and budgets that still require HW5 measurement are tracked in [[Pending_Measured_Constants_Register]].

## Current Priority

Reference Game expansion is paused.

Current documentation priority is HW5 hardware, Platform contracts, and bring-up procedure coverage.

Active bring-up CubeMX file:

- `firmware/peepshow_hw5_fw0/peepshow_hw5_fw0.ioc`

Reference/full-intent CubeMX file:

- `firmware/peepshow_hw5_fw1/peepshow_hw5_fw1.ioc`

The `fw1` `.ioc` is retained as the full intended target for reference. During hardware bring-up, `fw0` is the active generated firmware line until the phased bring-up scope catches up.

## Arrival-Day Rule

The historical HW5 process began with [[HW5_Arrival_Phase0_Checklist]].

This readiness note can confirm that documentation is prepared, but it does not prove hardware behavior. Subsystem behavior becomes known-good only from measured evidence linked in [[Brought_Up_Tracker]].

## Readiness Gates

| Gate | Required Notes | Status | Notes |
|---|---|---|---|
| pin authority | [[HW5_CubeMX_Pin_Map]], [[HW5_Pin_Ownership_Matrix]] | Drafted | must stay aligned with schematic, `fw0` bring-up scope, and `fw1` reference intent |
| part identity | [[HW5_Part_Tokens]] | Mostly resolved | exact rotary encoder model unknown; documented as TTC green encoder |
| power and safe defaults | [[HW5_Power_Rails]], [[PMIC_and_Power_Contract]] | Drafted | primary GPIO polarities recorded; NINA auxiliary pins changed to `GPIO_Analog` safe defaults in `fw1` reference `.ioc`; preserve equivalent safe behavior when introduced in `fw0` |
| clocks | [[HW5_Clock_Tree_Contract]], [[CubeMX_Configuration_Checklist]] | Drafted | SAI1 and OCTOSPI1 CubeMX baseline recorded; OCTOSPI performance tuning remains later work |
| DMA | [[HW5_DMA_Map]] | Drafted | buffer regions/alignment still need final firmware memory plan |
| wake sources | [[HW5_Wake_Sources]] | Drafted | encoder and sensor wake depth require measured proof |
| state machines | [[Subsystem_State_Machines]] and subsystem contracts | Drafted | transition tables can be expanded during implementation planning |
| bring-up evidence path | [[Brought_Up_Tracker]] plus runbooks | In progress | runbook coverage is drafted; measured HW5 evidence is still required |
| phased CubeMX bring-up | [[FW0_Phased_CubeMX_Bring-up_Plan]] | Drafted | `fw0` active, `fw1` reference only until promotion decision |

## Bring-Up Runbook Coverage

| Subsystem | Contract | Runbook | Status |
|---|---|---|---|
| power / PMIC / battery | [[PMIC_and_Power_Contract]] | [[ADP5360_Power_Bring-up_Runbook]] | drafted with safety-gated PMIC baseline |
| display | [[Display_and_Rendering_Contract]] | [[LS013B7DH05_Display_Bring-up_Runbook]] | drafted with pattern/mapping and LPBAM gate |
| external flash / storage | [[Storage_and_Installer_Contract]] | [[AT25SL128A_External_Flash_Bring-up_Runbook]] | drafted with scratch-only flash baseline |
| USB MSC / installer transport | [[Storage_and_Installer_Contract]] | [[USB_MSC_Bring-up_and_Recovery_Runbook]] | drafted |
| speaker / BBB audio | [[Audio_Contract]] | [[Audio_Output_Bring-up_Runbook]] | drafted with clock/DMA/BBB baseline |
| buttons / BOOT0 / Start MR path | [[Button_Input_Contract]] | [[Button_Input_Bring-up_Runbook]] | drafted with Start/BOOT0 hazards |
| rotary encoder | [[Rotary_Encoder_Input_Contract]] | [[Rotary_Encoder_Bring-up_Runbook]] | drafted with CubeMX/TIM2 baseline |
| light sensor | [[Light_Sensor_Contract]] | [[TEMT6000_Light_Sensor_Bring-up_Runbook]] | drafted with ADC/normalization baseline |
| IMU | [[IMU_Contract]] | [[LIS2DUX12_IMU_Bring-up_Runbook]] | drafted with ST baseline sequence |
| joystick / hall | [[Joystick_Hall_Input_Contract]] | [[TMAG3001_Joystick_Bring-up_Runbook]] | drafted with threshold-wake baseline |
| BLE / NINA | [[BLE_Communication_Contract]] | [[NINA_B112_BLE_Bring-up_Runbook]] | drafted with control-pin hazard notes |
| sleep and wake integration | [[Power_and_Sleep_Policy]], [[HW5_Wake_Sources]] | [[Sleep_Wake_Integration_Bring-up_Runbook]] | drafted with owner quiesce/wake matrix |

## Known Documentation Gaps

- Keep CubeMX GPIO modes for NINA auxiliary pins as `GPIO_Analog` when introduced: `NINA_SW1`, `NINA_SW2`, `NINA_DTR`, and `NINA_DSR` are safe high-Z/no-pull defaults unless intentionally reconfigured by `thComm`.
- Confirm generated GPIO init order preserves NINA analog defaults before releasing `NINA_NRST`.
- Confirm generated GPIO init drives active-high `VLT_LCD` low/disabled before display owner policy is ready.
- Define DMA buffer memory regions and alignment once memory layout is planned.
- Assign exact offset/size for the protected external flash fault-log ring region near the end of flash before implementing persistent fault evidence.
- BLE first bring-up uses interrupt-driven RX/TX static ring buffers; DMA remains deferred until throughput or CPU measurements justify it.
- Fill measured LPBAM display experiment results during display/sleep bring-up.
- Record LS013B7DH05 pixel polarity, line address format, row order, byte order, dirty granularity, and full-frame fallback threshold from measured pattern tests.
- Record AT25SL128A JEDEC/device ID bytes, status/config register meanings used by the driver, scratch test range, and deep-power-down wake timing from measured HW5 tests.
- Confirm ADP5360 uses the public 7-bit I2C address convention from [[Platform_Hardware_Abstraction_Contract]], then record threshold constants, PMIC interrupt polarity/clear behavior, VBUS cross-check behavior, and safe evidence for low/critical battery policy.
- Record speaker DMA buffer sizing, MAX98357A enable settle timing, BBB frequency/duration bounds, and measured music plus 5 SFX mixer budget.
- Record sleep-class current, wake latency, resume latency, owner quiesce timeouts, and unknown-wake evidence for every supported runtime class.
- Complete the LIS2DUX12 post-baseline optimization pass so step counting is proven in the lowest-power polling mode, not only the ST 25 Hz/+/-4 g bring-up configuration.
- Measure TMAG3001 threshold values, hysteresis, sleep interval, axis mapping, and final `JOY_INT` polarity before implementing joystick wake policy.
- Measure rotary encoder settle time, sign convention, detent ratio, input filtering needs, acceleration knobs, and wake-armed current before finalizing input policy.
- Measure TEMT6000 settle time, dark/room/bright ADC values, normalized bands, filtering policy, and streaming limits before finalizing light-sensor API behavior.
- Verify generated EXTI edge configuration, button logic levels, debounce timing, chord/repeat behavior, Start shipping-prep thresholds, and BOOT0 application/ROM boundary on HW5.

## Before CubeMX Generation Or Regeneration Checklist

1. Confirm whether the change applies to active bring-up `fw0` or reference/full-intent `fw1`.
2. Resolve pin ownership conflicts between [[HW5_CubeMX_Pin_Map]] and [[HW5_Pin_Ownership_Matrix]].
3. Confirm all wake-capable EXTI paths are documented in [[HW5_Wake_Sources]].
4. Confirm every physical subsystem has a bring-up runbook.
5. Confirm every runbook says what evidence must be recorded in [[Brought_Up_Tracker]].
6. Confirm all safe defaults are either resolved or explicitly marked as bring-up unknowns.
7. Confirm the Platform owner thread for every peripheral is documented.
8. Confirm CubeMX generated initial output states cannot violate NINA startup/reset or display OE policy.

## Rule

A subsystem is not known-good until measured evidence is linked from [[Brought_Up_Tracker]].
