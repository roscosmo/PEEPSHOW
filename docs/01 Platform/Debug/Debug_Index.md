# Debug Index

This section defines observability requirements, fault capture, SWD/SWO discipline, and bring-up debug workflow.

## Core Notes

- [[Debug_and_Observability]]
- [[Diagnostics_API_Contract]]
- [[USB_Development_Mode_Contract]]
- [[Live_Tuning_And_Knobs_Contract]]
- [[Telemetry_And_Debug_Dashboard_Contract]]
- [[Tracealyzer_Snapshot_Evidence_Contract]]
- [[Debug_Workflows]]
- [[Boot_and_Fault_Supervisor_State_Machine]]
- [[Brought_Up_Tracker]]

## Core Rule

Bring-up claims require evidence.

Unknown wake causes, intermittent storage failures, and unexplained faults remain open defects until trace, fault, or measurement evidence explains them.

The package-facing diagnostics API boundary is defined in [[Diagnostics_API_Contract]].
