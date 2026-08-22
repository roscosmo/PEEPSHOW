# Development Tooling Index

This section defines host-side and firmware-facing development tools for PeepShow.

Development tools support bring-up, package authoring, telemetry, live-safe Platform tuning, replay, and developer workflows. They must not become hidden game APIs or bypass Platform ownership.

## Core Notes

- [[Dev_Orchestration_CLI_Contract]]
- [[Authoring_Tool_Architecture]]
- [[Authoring_Project_Schema_Contract]]
- [[HW6_Authoring_Vertical_Slice]]
- [[USB_Development_Mode_Contract]]
- [[Live_Tuning_And_Knobs_Contract]]
- [[Telemetry_And_Debug_Dashboard_Contract]]
- [[Tracealyzer_Snapshot_Evidence_Contract]]
- [[Evidence_Artifact_Convention]]
- [[Knobs_and_Tuning_Contract]]
- [[Debug_and_Observability]]
- [[Debug_Workflows]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]

## Tooling Lanes

Development tooling is split into four lanes:

| Lane | Purpose | Primary Interfaces |
|---|---|---|
| observation | understand what the device is doing | [[Telemetry_And_Debug_Dashboard_Contract]], SWO structured events, Tracealyzer snapshots, staged log export, optional CDC telemetry |
| control | change live-safe Platform development settings | [[Live_Tuning_And_Knobs_Contract]] over dev-only CDC or digital twin adapters |
| authoring | produce valid content and packages | asset pipeline, package compiler, content parameter editors, validators |
| replay | reproduce runtime behavior | digital twin, trace capture, deterministic input/time/sensor streams |

The recommended host entry point for routine workflows is [[Dev_Orchestration_CLI_Contract]].

Visual authoring tools should use [[Authoring_Project_Schema_Contract]] as their editable source model and [[Asset_Pipeline_and_Package_Tooling_Contract]] as the package build boundary.

The V1 desktop process, UI, Python service, authoring preview, and package-build
boundaries are defined by [[Authoring_Tool_Architecture]].

The first concrete authoring acceptance path is [[HW6_Authoring_Vertical_Slice]]. It proves one authored Reference Game package from project source through deterministic compilation, installation, runtime transitions, and measured HW6 power evidence.

## Boundary

Tools may make development faster.

Tools must not:

- mutate Platform state through raw memory pokes as the normal workflow
- bypass owner-thread request paths
- expose hardware controls to package/game tools
- make USB MSC and firmware storage access share the same FAT volume at the same time
- treat digital twin or host tooling output as hardware bring-up evidence

## V1 Priorities

1. keep USB MSC as the normal user package-install/export workflow
2. use SWO for structured observation during bring-up
3. add USB CDC only as a mutually exclusive developer personality
4. make live tuning generated, typed, validated, and owner-routed
5. build dashboards and replay tooling from the same structured events used by the digital twin
6. wrap common generate/build/package/flash/trace/twin/evidence sequences in a single auditable CLI

- [[PPK2_Capture_Tool]]
