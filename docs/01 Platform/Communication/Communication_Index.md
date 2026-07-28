# Communication Index

This section defines game-agnostic communication interfaces.

The active HW6 communication path covers the retained BLE/NINA module over `LPUART1` with hardware flow control and module control GPIO. Its behavior remains pending HW6 revalidation.

## Core Notes

- [[BLE_Communication_Contract]]
- [[Communication_API_Contract]]
- [[RTOS_Ownership_and_Queue_Topology]]
- [[Power_and_Sleep_Policy]]
- [[HW6_CubeMX_Pin_Map]]
- [[HW6_Pin_Ownership_Matrix]]

## Boundary

The Platform owns the BLE module, UART, reset/mode pins, flow-control behavior, and power transitions.

The Engine may request generic communication capabilities.

The Reference Game may consume approved Engine communication APIs, but it must not configure BLE hardware directly.

The package-facing API boundary is defined in [[Communication_API_Contract]].
