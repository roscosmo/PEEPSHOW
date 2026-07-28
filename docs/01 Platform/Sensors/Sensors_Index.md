# Sensors Index

This section defines sensor ownership, sensor snapshots, wake events, recovery behavior, and Engine-facing sensor abstractions.

## Core Notes

- [[Subsystem_State_Machines]]
- [[Peripheral_Robustness_Contract]]
- [[Light_Sensor_Contract]] - retired HW5 implementation; light capabilities are blocked on HW6
- [[IMU_Contract]]
- [[Power_and_Sleep_Policy]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Wake_Sources]]

## Boundary

The Platform owns sensor buses, IRQs, configuration, power state, and recovery.

The Engine and Reference Game request sensor capabilities or consume snapshots through approved APIs.

The package-facing API boundary is defined in [[Sensor_API_Contract]].
