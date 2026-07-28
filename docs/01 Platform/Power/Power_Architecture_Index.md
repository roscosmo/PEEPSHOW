# Power Architecture Index

This section defines sleep classes, clock policy, wake classification, and runtime intent mapping.

## Core Notes

- [[PMIC_and_Power_Contract]]
- [[Power_and_Sleep_Policy]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Authority_and_Invariants]]
- [[Subsystem_State_Machines]]
- [[HW6_Wake_Sources]]
- [[HW6_Power_Rails]]

## Core Rule

Runtime and game layers express intent.

The Platform decides sleep depth, clock profile, wake-source arming, and resume policy.

HW6 PMIC, battery, VBUS, and shipping-mode behavior is defined in [[PMIC_and_Power_Contract]] and remains pending HW6 evidence.
