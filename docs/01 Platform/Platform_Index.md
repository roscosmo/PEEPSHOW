# Platform Index

The Platform is the console layer.

It owns hardware behavior, RTOS structure, mode policy, low-power behavior, peripheral ownership, storage, display, audio, input, sensors, communication, diagnostics, and bring-up-facing abstractions.

The Platform is game-agnostic.

## Authoritative Platform Contracts

- [[Authority_and_Invariants]]
- [[Architecture_and_Boundaries]]
- [[Platform_Freeze_Charter]]
- [[Repository_Structure]]
- [[Interface_Control_Document]]
- [[Platform_Hardware_Abstraction_Contract]]
- [[Subsystem_State_Machines]]
- [[Peripheral_Robustness_Contract]]
- [[Memory_and_Budgeting_Contract]]
- [[Knobs_and_Tuning_Contract]]
- [[Platform_Firmware_Update_and_Development_Security_Policy]]

## Core Subsystems

- [[RTOS_Index]]
- [[Power_Architecture_Index]]
- [[Rendering_Index]]
- [[Audio_Contract]]
- [[Storage_Index]]
- [[Input_Index]]
- [[Sensors_Index]]
- [[Communication_Index]]
- [[Debug_and_Observability]]
- [[USB_Development_Mode_Contract]]
- [[Validation_Index]]
- [[Shell_Settings_Calibration_Contract]]
- [[Shell_and_UI_Navigation_State_Machine]]
- [[Boot_and_Fault_Supervisor_State_Machine]]
- [[File_Ownership]]

## Hardware Boundary

The Platform consumes the board contract from [[Hardware_Index]].

CubeMX output must match the hardware and platform contracts. If CubeMX output disagrees with the vault, either CubeMX is wrong or the vault must be explicitly updated with a reason.

## Game Boundary

Game and Reference Game notes may request abstract capabilities from Platform through Engine APIs.

They may not define:

- peripheral ownership
- RTOS thread topology
- CubeMX pin assignments
- DMA policy
- clock policy
- sleep policy
- storage ownership
- fault handling
