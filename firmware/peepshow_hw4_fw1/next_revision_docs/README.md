# PeepOS Documentation Set

This folder is the documentation baseline for this project.

It is platform-first by design:
- Finish PeepOS as a low-power handheld shell OS first.
- Keep package logic above the platform boundary.
- Do not embed a specific game engine in core firmware.

---

## Reading Order (Required)

1. `01_project_charter.md`
2. `02_architecture_and_boundaries.md`
3. `03_repo_structure.md`
4. `04_bootstrap_and_build.md`
5. `14_authority_and_invariants.md`
6. `05_subsystem_state_machines.md`
7. `27_shell_ui_navigation_state_machine.md`
8. `28_package_manager_state_machine.md`
9. `29_boot_and_fault_supervisor_state_machine.md`
10. `30_runtime_host_internal_state_machines.md`
11. `06_runtime_host_contract.md`
12. `07_package_contract.md`
13. `08_power_and_sleep_policy.md`
14. `15_hardware_revision_contract.md`
15. `16_rtos_ownership_and_queue_topology.md`
16. `17_display_rendering_contract.md`
17. `18_audio_contract.md`
18. `19_storage_and_installer_contract.md`
19. `31_usb_msc_bringup_and_recovery.md`
20. `20_peripheral_robustness_contract.md`
21. `21_knobs_contract.md`
22. `09_interface_control.md`
23. `22_bringup_spec_vs_tracker.md`
24. `23_brought_up_tracker_template.md`
25. `26_brought_up_archive_template.md`
26. `24_debug_workflows.md`
27. `10_validation_plan.md`
28. `11_memory_and_budgeting.md`
29. `12_debug_observability.md`
30. `25_asset_pipeline_and_package_tooling_contract.md`
31. `13_architecture_decisions.md`

---

## Platform Freeze Definition

Before game-specific development starts, the firmware must be complete as:
- clock and home shell
- settings and calibration
- package browser and installer entry
- runtime host manager
- shared UX services
- validated low-power behavior

No game-specific engine work should begin before this freeze point is complete and signed off.

---

## What This Set Emphasizes

This set adds guidance required to keep implementation boundaries stable:
- explicit per-subsystem/peripheral state machines
- interface control document for inter-thread messages
- memory and retained-RAM budgeting contract
- architecture decision record process
- explicit knobs contract and generator flow
- visibility-gated public/private knobs with owner-thread enforcement
- explicit bring-up spec versus brought-up tracker split
- dedicated hardware/peripheral contracts for the target board
- dedicated USB MSC bring-up and recovery runbook

---

## Document Authority

For this project, use this priority order:
1. `02_architecture_and_boundaries.md`
2. `14_authority_and_invariants.md`
3. `05_subsystem_state_machines.md`
4. `06_runtime_host_contract.md` and `07_package_contract.md`
5. remaining docs in this folder

If documents conflict, update them immediately so only one rule exists.
