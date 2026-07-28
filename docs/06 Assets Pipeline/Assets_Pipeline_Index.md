# Assets Pipeline Index

This section defines game-agnostic asset production, package generation, validation, and deterministic tooling.

## Core Notes

- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Package_Compatibility_Report_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[Rendering_API_Contract]]
- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Runtime_Host_Contract]]

## Core Rule

Tools target Engine and package contracts.

Tools must validate all content before package compilation or export.

Tools must not encode target-specific HAL, RTOS, DMA, clock, or peripheral assumptions.
