# Input Index

This section defines input ownership, raw event capture, logical input events, and focus handoff to Platform shell or Engine systems.

## Core Notes

- [[Subsystem_State_Machines]]
- [[RTOS_Ownership_and_Queue_Topology]]
- [[Interface_Control_Document]]
- [[Shell_and_UI_Navigation_State_Machine]]
- [[Button_Input_Contract]]
- [[Rotary_Encoder_Input_Contract]] - retired HW5 implementation; `input.encoder` is blocked on HW6
- [[Joystick_Hall_Input_Contract]]
- [[Input_Focus_API_Contract]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Wake_Sources]]

## Boundary

The Platform owns raw input capture, debounce, calibration, and wake behavior.

The Engine consumes logical input events and focus abstractions.

The Reference Game consumes Engine input actions.
