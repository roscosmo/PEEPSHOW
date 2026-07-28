# Repository Structure

Status: `active`

This is the repository layout contract for PeepShow documentation, firmware
targets, development tools, and future package-authoring implementation.

The repository root is hardware-independent. HW6 is the active hardware target;
HW5 is retained only as historical implementation and validation context.

Related:

- [[Authority_and_Invariants]]
- [[Architecture_and_Boundaries]]
- [[File_Ownership]]
- [[Hardware_Index]]
- [[HW6_Hardware_Documentation_Readiness]]

---

## Canonical Root

```text
PEEPSHOW/
|-- .gitattributes
|-- .gitignore
|-- .gitmodules
|-- README.md
|-- docs/
|   |-- .obsidian/
|   |-- 00 Inbox/
|   |-- 01 Platform/
|   |-- 02 Bring-up/
|   |-- 03 Engine API/
|   |-- 04 Game Design/
|   |-- 05 Reference Game/
|   |-- 06 Assets Pipeline/
|   |-- 07 Hardware/
|   |-- 08 Research/
|   |-- 09 Canvases/
|   |-- 10 References/
|   |-- 11 Development Tools/
|   `-- assets/
|-- firmware/
|   |-- peepshow_hw5_fw0/
|   |-- peepshow_hw5_fw1/
|   `-- peepshow_hw6_fw1/
`-- tools/
    |-- ppk2_capture.py
    |-- ppk2_console.py
    |-- ppk2_service.py
    |-- requirements-ppk2.txt
    `-- vendor/
        `-- ppk2-api-python/
```

The future safe-arrival HW6 target may be created as
`firmware/peepshow_hw6_fw0/` by the approved HW6 bring-up workflow. Its absence
before generation is intentional; do not add an empty placeholder project.

---

## Firmware Target Roles

| Path | Status | Role |
|---|---|---|
| `firmware/peepshow_hw5_fw0/` | `retired_reference` | HW5 phased bring-up, low-power, and LPBAM experiment implementation |
| `firmware/peepshow_hw5_fw1/` | `retired_reference` | HW5 full-intent CubeMX IOC reference |
| `firmware/peepshow_hw6_fw0/` | `planned` | minimal HW6 safe-arrival and staged bring-up target |
| `firmware/peepshow_hw6_fw1/` | `pending_validation` | imported HW6 full-intent CubeMX design input; not yet generated or known-good firmware |

Rules:

- Each CubeMX target remains self-contained under its target directory.
- Generated `Core`, `Drivers`, `Middlewares`, linker, startup, and CubeMX files
  belong to that target, not directly under `firmware/`.
- Do not flash or repurpose an HW5 target as an HW6 image.
- HW5 source may be consulted for proven behavior, but HW6 capability remains
  pending until target-qualified evidence exists.
- Build directories and local probe output are generated artifacts and are not
  repository content.

---

## Documentation

`docs/` is the Obsidian vault and architecture source of truth.

It owns:

- Platform and Engine contracts
- hardware revision contracts
- bring-up plans, runbooks, trackers, and reviewed evidence
- package and authoring contracts
- Reference Game design
- development-tool contracts

Machine-local Obsidian workspace state is ignored. Shared vault configuration
and intentionally selected plugins may be versioned.

---

## Development Tools

`tools/` contains repository-level host tools. Local Python environments and
caches are not repository content.

`tools/vendor/ppk2-api-python/` is an external Git submodule pinned through the
root `.gitmodules`. Do not vendor its nested Git history as ordinary files.

---

## Local-Only Data

The following current directories are deliberately ignored:

| Path | Reason |
|---|---|
| `backups/` | local safety copies must not become duplicate authority |
| `blend3d/` | large CAD, mesh, and fabrication-source binaries require separate archival or Git LFS storage |
| `tools/.venv/` | machine-specific generated Python environment |
| `firmware/*/build/` | generated build output with machine-specific paths |

Reviewed hardware and power evidence belongs under the evidence convention in
`docs/02 Bring-up/Evidence/`, not in arbitrary root log folders.

---

## Future Implementation Roots

Do not create empty directory trees solely to imply architecture. Add canonical
source roots when their first implementation change defines a real build and
ownership boundary.

Expected future concerns include:

- reusable PeepOS Platform source outside retired target-specific experiments
- reusable Engine runtime source
- package and Reference Game authored source
- authoring compiler, validator, preview, and editor source
- target profiles and generated compatibility artifacts

Before adding one of these roots, update this contract and [[File_Ownership]] in
the same change.

---

## Naming And Ownership

- Target directories use `peepshow_hw<revision>_fw<role>`.
- Hardware-independent source must not be named after HW5 or HW6.
- Generated output must not become an architecture authority.
- Every implementation module records its owner, allowed dependencies,
  forbidden dependencies, and required validation in [[File_Ownership]].
- Platform, Engine, package, and Reference Game boundaries follow
  [[Architecture_and_Boundaries]].

