# Hardware Index

This section is the board authority for the active HW6 target and preserves HW5 as retired historical context.

It records the schematic/PCB/BOM basis, canonical part tokens, pin ownership, clock tree, DMA map, power rails, wake sources, and CubeMX configuration expectations.

## Active Hardware: HW6

- [[HW6_Hardware_Documentation_Readiness]]
- [[HW6_Hardware_Revision_Contract]]
- [[HW6_Delta_From_HW5]]
- [[HW6_Part_Tokens]]
- [[HW6_CubeMX_Pin_Map]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Clock_Tree_Contract]]
- [[HW6_DMA_Map]]
- [[HW6_Power_Rails]]
- [[HW6_Wake_Sources]]
- [[CubeMX_Configuration_Checklist]]

The imported full-intent design input is `firmware/peepshow_hw6_fw1/PEEPSHOW_HW6_FW1.ioc`. It is not yet an HW6 known-good generated firmware target.

## Retired Hardware: HW5

- [[HW5_Hardware_Documentation_Readiness]]
- [[HW5_Hardware_Revision_Contract]]
- [[HW5_Part_Tokens]]
- [[HW5_CubeMX_Pin_Map]]
- [[HW5_Pin_Ownership_Matrix]]
- [[HW5_Clock_Tree_Contract]]
- [[HW5_DMA_Map]]
- [[HW5_Power_Rails]]
- [[HW5_Wake_Sources]]

HW5 documents and evidence remain historical references. They do not grant HW6 capabilities.

## Visual Maps And Assets

- [[Hardware_Canvas_Index]]
- [[HW5_Hardware_Visual_Map]] - retired HW5 visual reference
- [[Hardware_Asset_Export_Guide]]

An HW6 visual map remains pending until the final fabrication package is imported and reviewed.

## Active Bring-Up Coverage

- [[ADP5360_Power_Bring-up_Runbook]]
- [[LS013B7DH05_Display_Bring-up_Runbook]]
- [[AT25SL128A_External_Flash_Bring-up_Runbook]]
- [[USB_MSC_Bring-up_and_Recovery_Runbook]]
- [[Audio_Output_Bring-up_Runbook]] - speaker path only on HW6
- [[Button_Input_Bring-up_Runbook]]
- [[LIS2DUX12_IMU_Bring-up_Runbook]]
- [[TMAG3001_Joystick_Bring-up_Runbook]]
- [[NINA_B112_BLE_Bring-up_Runbook]]
- [[Sleep_Wake_Integration_Bring-up_Runbook]]

The rotary encoder, TEMT6000 light sensor, and physical PAM/piezo path were removed from HW6. Their HW5 runbooks remain historical and must not be scheduled as HW6 bring-up requirements.

## Required Rule

The active target `.ioc`, generated firmware, hardware contracts, and measured evidence must agree.

If CubeMX generation changes pin ownership, clocks, DMA, or wake behavior, update the HW6 authority documents before firmware architecture work continues.
