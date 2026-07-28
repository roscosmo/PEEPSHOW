# Engine API Index

The Engine sits between Platform and Game.

It owns reusable runtime abstractions, host lifecycle, package/content contracts, scene/runtime behavior, asset loading, and game-development APIs.

The Engine may understand gameplay concepts abstractly, but it must not depend on the Reference Game.

## Core Notes

- [[Game_Authoring_API_Contract]]
- [[Authoring_Project_Schema_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Audio_API_Contract]]
- [[Input_Focus_API_Contract]]
- [[Rendering_API_Contract]]
- [[Sensor_API_Contract]]
- [[Communication_API_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Diagnostics_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Runtime_Host_Contract]]
- [[Runtime_Host_Internal_State_Machines]]
- [[Package_Contract]]
- [[Package_Manager_State_Machine]]
- [[Package_Asset_Loading_API_Contract]]
- [[Package_Save_Settings_API_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Package_Compatibility_Report_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]

## Boundary

The Engine consumes Platform capabilities.

The Reference Game consumes Engine APIs.

The Engine must remain reusable for games other than the Reference Game.
