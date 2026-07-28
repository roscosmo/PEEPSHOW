# Bring-up Index

Bring-up documentation records how the active HW6 target becomes known-good while preserving HW5 results as retired historical evidence.

This section is operational and historical. It tracks validation sequences, failures, fixes, temporary measures, and evidence.

## Active HW6 Notes

- [[HW6_Hardware_Documentation_Readiness]]
- [[HW6_Arrival_Phase0_Checklist]]
- [[HW6_Revalidation_Matrix]]
- [[HW6_Brought_Up_Tracker]]
- [[Brought_Up_Tracker]]
- [[Evidence_Artifact_Convention]]
- [[Pending_Measured_Constants_Register]]
- [[Validation_Plan]]
- [[CubeMX_Configuration_Checklist]]

## Shared Procedures And Tooling

- [[Dev_Orchestration_CLI_Contract]]
- [[Bootstrap_and_Build]]
- [[Bring-up_Spec_vs_Tracker]]
- [[Brought_Up_Archive]]
- [[Debug_Workflows]]
- [[Bounded_Build_Flash_Debug_Runbook]]
- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[USB_Development_Mode_Contract]]
- [[Live_Tuning_And_Knobs_Contract]]
- [[Telemetry_And_Debug_Dashboard_Contract]]
- [[Tracealyzer_Snapshot_Evidence_Contract]]

## Active HW6 Subsystem Runbooks

- [[ADP5360_Power_Bring-up_Runbook]]
- [[LS013B7DH05_Display_Bring-up_Runbook]]
- [[AT25SL128A_External_Flash_Bring-up_Runbook]]
- [[USB_MSC_Bring-up_and_Recovery_Runbook]]
- [[Audio_Output_Bring-up_Runbook]] - speaker path only
- [[Button_Input_Bring-up_Runbook]]
- [[LIS2DUX12_IMU_Bring-up_Runbook]]
- [[TMAG3001_Joystick_Bring-up_Runbook]]
- [[NINA_B112_BLE_Bring-up_Runbook]]
- [[Sleep_Wake_Integration_Bring-up_Runbook]]

## Retired HW5 Material

- [[HW5_Brought_Up_Tracker]]
- [[HW5_Arrival_Phase0_Checklist]]
- [[FW0_Phased_CubeMX_Bring-up_Plan]]
- [[HW5_Hardware_Documentation_Readiness]]
- [[Rotary_Encoder_Bring-up_Runbook]]
- [[TEMT6000_Light_Sensor_Bring-up_Runbook]]

HW5 results are regression references only. Do not copy an HW5 result into the HW6 tracker as a pass.

## Bring-up Rule

Do not mark a phase complete from assumption.

A phase is complete only when the expected behavior is measured on the identified HW6 board and target-qualified evidence is linked from [[HW6_Brought_Up_Tracker]].

## Digital Twin Rule

The [[Digital_Twin_Host_Runtime_Contract]] is post-validation work.

The host digital twin is implemented from measured PeepShow Platform behavior after active-target Platform validation is complete. It can validate Engine, package, game-logic, replay, and contract-parity behavior, but it cannot provide hardware bring-up evidence.
