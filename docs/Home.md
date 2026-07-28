# PeepShow Vault Home

This vault is the architecture and bring-up source of truth for PeepShow. HW6 is the active hardware target; HW5 is retired historical context.

Start here:

- [[README]]
- [[Authority_and_Invariants]]
- [[Brought_Up_Tracker]]
- [[Pending_Measured_Constants_Register]]
- [[Validation_Plan]]

## Architecture

```text
Platform -> Engine -> Reference Game
```

- [[Platform_Index]]
- [[Engine_API_Index]]
- [[Game_Design_Index]]
- [[Reference_Game_Index]]

The Platform owns hardware behavior.

The Engine owns reusable package/game APIs.

The Reference Game proves capability but does not define Platform policy.

## Work Areas

- [[Bring-up_Index]]
- [[Hardware_Index]]
- [[Assets_Pipeline_Index]]
- [[Development_Tooling_Index]]
- [[Reference_Index]]

## Platform Subsystems

- [[RTOS_Index]]
- [[Power_Architecture_Index]]
- [[Rendering_Index]]
- [[Audio_Index]]
- [[Storage_Index]]
- [[Input_Index]]
- [[Sensors_Index]]
- [[Communication_Index]]
- [[Debug_Index]]
- [[Validation_Index]]

## Current Rule

Do not mark hardware behavior known-good without target-qualified evidence. Active HW6 evidence is linked from [[HW6_Brought_Up_Tracker]]; [[Brought_Up_Tracker]] routes to target-specific ledgers.
