# PeepShow

This is the hardware-independent PeepShow workspace. HW6 is the active hardware
target. HW5 is retained as historical implementation and validation evidence.

Project documentation lives in the Obsidian vault at
[docs/Home.md](docs/Home.md). Open `docs/` as the vault root in Obsidian.

## Workspace Layout

- `docs/` - architecture, contracts, hardware documentation, bring-up plans,
  evidence, and game/tooling design.
- `firmware/peepshow_hw5_fw0/` - retired HW5 phased bring-up and LPBAM
  experiment implementation.
- `firmware/peepshow_hw5_fw1/` - retired HW5 full-intent CubeMX reference IOC.
- `firmware/peepshow_hw6_fw1/` - active HW6 full-intent CubeMX design input.
  It is not yet generated or validated firmware.
- `tools/` - host-side development and measurement tools.

`backups/` and `blend3d/` are local-only working data and are intentionally
excluded from source control. Large CAD and fabrication assets should use
separate archival or Git LFS storage rather than the firmware repository.

## Current Phase

HW6 documentation and safe-arrival preparation are active. HW6 capabilities
remain pending until they are measured on physical HW6 hardware and recorded in
the bring-up evidence.

## Repository Initialization

The root `.gitattributes` is authoritative for line endings. On Windows, create
the repository with local automatic CRLF conversion disabled:

```powershell
git init -b main
git config core.autocrlf false
git add .
git status --short
git submodule status
```

Inspect the staged inventory before creating the first commit or adding a new
remote. Keep the retired HW5 repository archive until the new repository is
committed and pushed successfully.
