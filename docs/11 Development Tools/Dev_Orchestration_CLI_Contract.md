# Development Orchestration CLI Contract

This document defines the project-level command-line tool that coordinates PeepShow development workflows.

The orchestration CLI is a developer convenience layer. It calls existing generators, validators, build tools, flash/debug tools, package tools, telemetry tools, and digital twin workflows. It does not become a new architecture authority.

Related:

- [[Development_Tooling_Index]]
- [[Bootstrap_and_Build]]
- [[Debug_Workflows]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Package_Blob_Format_Contract]]
- [[USB_Development_Mode_Contract]]
- [[Live_Tuning_And_Knobs_Contract]]
- [[Telemetry_And_Debug_Dashboard_Contract]]
- [[Tracealyzer_Snapshot_Evidence_Contract]]
- [[Evidence_Artifact_Convention]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Brought_Up_Tracker]]
- [[Validation_Plan]]

---

## Scope

Defines:

- CLI responsibility and boundaries
- expected command families
- artifact and evidence conventions
- profile handling
- host/device safety rules

Does not define:

- exact CLI implementation language
- exact argument parser
- low-level flash/debug command syntax
- package binary schemas
- firmware architecture

---

## Core Principle

The CLI orchestrates documented workflows.

```text
peep command
    |
generators / validators / build tools / flash tools / dev protocols
    |
versioned artifacts and evidence
```

Rules:

- the CLI must not bypass validation gates.
- the CLI must not invent hidden build modes.
- the CLI must not write generated outputs by hand.
- the CLI must not directly mutate firmware memory as the normal workflow.
- the CLI must surface exact commands and artifacts used for evidence.

---

## Command Namespace

Use one top-level command namespace for project workflows.

Conceptual examples:

```text
peep doctor
peep generate platform-knobs
peep validate docs
peep validate package
peep build firmware
peep flash firmware
peep debug openocd
peep package build
peep package install-msc
peep package upload-cdc
peep platform-update stage-msc
peep platform-update upload-cdc
peep trace snapshot
peep telemetry capture
peep dashboard
peep twin run
peep evidence add
```

Final command names may change, but command responsibilities should remain stable.

---

## Command Families

| Family | Purpose |
|---|---|
| `doctor` | check host prerequisites and project paths |
| `generate` | run deterministic generators such as Platform knobs, schemas, docs metadata |
| `validate` | run docs, schema, package, and contract checks |
| `build` | configure/build firmware or tools through the selected build system |
| `flash` | program firmware through approved debug/programming tools |
| `debug` | start approved debug sessions or helper scripts |
| `package` | build, validate, install, or upload `PeepPkg` artifacts |
| `platform-update` | stage or upload Platform update artifacts through documented update policy, when supported |
| `usb` | inspect or select allowed USB development workflows |
| `trace` | prepare/dump Tracealyzer snapshot evidence |
| `telemetry` | capture/export dashboard telemetry artifacts |
| `dashboard` | launch host dashboard against a selected source |
| `twin` | run digital twin preview, replay, or validation |
| `evidence` | create evidence records and attach artifact paths |

Rules:

- commands must use documented contracts as their authority.
- commands that mutate device state must require explicit target/profile selection.
- commands that write flash or install packages must print the selected device, build/profile, package checksum, and action summary before execution.
- destructive or irreversible operations require explicit confirmation or a dedicated noninteractive CI flag.

---

## Profiles

The CLI must make profiles explicit.

Required profile dimensions:

| Dimension | Examples |
|---|---|
| build profile | `Debug`, `Release`, `trace`, `dev_cdc`, `shipping` |
| target profile | `HW6_PENDING_VALIDATION`, `HW6_VALIDATED_BASELINE`, `HOST_DIGITAL_TWIN_HW6` |
| package build profile | `authoring_preview`, `dev_package`, `release_candidate`, `shipping` |
| transport profile | `swd`, `swo`, `msc`, `cdc_dev`, `twin` |

Rules:

- default profiles must be visible.
- evidence artifacts must record active profiles.
- commands must reject incompatible profile combinations.
- shipping builds must not silently enable dev CDC, verbose dashboard telemetry, or live tuning.

---

## Artifact Model

Every CLI workflow should produce or reference deterministic artifacts.

Common artifacts:

- firmware `.elf`
- firmware `.bin`
- map file
- generated headers
- generated schema/code reports
- package blob
- package compatibility report
- telemetry capture bundle
- Tracealyzer snapshot artifact
- dashboard export
- live-tuning overlay
- digital twin replay report
- flash/debug log
- evidence record

Rules:

- artifact paths must be recorded in command output.
- artifact directories must be predictable.
- command output should include schema/tool versions where relevant.
- generated artifacts must not contain hidden nondeterministic timestamps unless explicitly declared.
- CLI commands should be dry-run capable where practical.

---

## Evidence Workflow

The CLI should make evidence capture easier but not automatic truth.

Evidence records should include:

- date/time
- board revision
- firmware commit
- knobs hash/version
- command line
- build profile
- target profile
- test case ID
- artifact paths
- pass/fail result
- notes
- issue IDs for failures

Rules:

- `peep evidence add` may prepare tracker-ready entries, but a human still verifies claims before marking bring-up complete.
- hardware known-good claims require measured evidence from the active target, currently HW6.
- digital twin and dashboard artifacts are evidence only for the classes allowed by their contracts.
- trace-enabled runs must record trace configuration.
- live-tuned runs must record active tuning overlay.

---

## Package Workflows

Package commands must follow the package and asset pipeline contracts.

Expected commands:

```text
peep package build --profile dev_package
peep package validate --target HW6_PENDING_VALIDATION
peep package install-msc --package path/to/game.peeppkg
peep package upload-cdc --package path/to/game.peeppkg
```

Rules:

- package build runs tool-side validation before emitting installable output.
- MSC install workflow copies to staging/export only; host owns MSC while mounted.
- CDC upload uses [[USB_Development_Mode_Contract]] and firmware-owned staging.
- no package command may bypass install-time validation.
- package checksums and compatibility reports must be retained.

---

## Platform Update Workflows

Platform update commands are future/deferred unless the Platform update policy and recovery path are implemented.

Expected future command shape:

```text
peep platform-update stage-msc --artifact path/to/peepshow-update.bin
peep platform-update upload-cdc --artifact path/to/peepshow-update.bin
```

Rules:

- Platform update artifacts are not `PeepPkg` packages.
- MSC staging copies the artifact only; firmware-owned Platform update validation starts after staging is reclaimed.
- CDC upload writes only to firmware-owned staging through documented USB developer commands.
- no CLI platform-update command may raw-program Platform firmware regions through CDC.
- direct firmware flashing remains under the `flash` family and uses approved debug/programming tools.
- apply/commit commands remain deferred until [[Platform_Firmware_Update_and_Development_Security_Policy]] defines the recovery/update flow.

---

## Trace And Telemetry Workflows

Trace and telemetry commands coordinate existing contracts.

Expected commands:

```text
peep trace snapshot --scenario sleep-wake
peep telemetry capture --source cdc_dev --duration 10s
peep dashboard --source twin
peep dashboard --source capture path/to/capture
```

Rules:

- Tracealyzer snapshot workflows follow [[Tracealyzer_Snapshot_Evidence_Contract]].
- dashboard telemetry follows [[Telemetry_And_Debug_Dashboard_Contract]].
- telemetry capture commands must record schema versions.
- dashboard commands must label hardware live, exported hardware, Tracealyzer-linked, or digital twin sources.
- trace and telemetry artifacts do not prove physical current or electrical behavior.

---

## Live Tuning Workflows

Live tuning commands must use [[Live_Tuning_And_Knobs_Contract]].

Expected commands:

```text
peep platform-knob list
peep platform-knob get platform.knobs.input.debounce_ms
peep platform-knob set platform.knobs.render.dither_mode bayer2x2
peep platform-knob overlay export
```

Rules:

- commands operate on stable paths, not raw addresses.
- non-live-safe Platform knobs are rejected.
- owner apply results must be surfaced as applied, staged, clamped, rejected, requires reboot, or unsupported.
- overlay export must record firmware commit and knobs hash.
- source update remains explicit; live overlay changes are not silently committed to `config/knobs.json`.
- package content parameters and package settings are not edited through `peep platform-knob`.

---

## Digital Twin Workflows

Twin commands must follow [[Digital_Twin_Host_Runtime_Contract]].

Expected commands:

```text
peep twin run --package path/to/game.peeppkg
peep twin replay --trace path/to/replay
peep twin validate --profile HOST_DIGITAL_TWIN_HW6
```

Rules:

- hardware-derived twin profiles are blocked until HW6 Platform validation evidence exists.
- twin output may validate package, runtime, replay, and dashboard behavior.
- twin output must not mark hardware behavior known-good.
- replay commands must record package checksum, input/time/sensor traces, target profile, and active tuning overlay.

---

## Safety Boundaries

The CLI must not expose:

- arbitrary shell access as a firmware command
- raw memory write as normal tuning
- raw flash write outside approved flash/programmer workflows
- direct filesystem access into protected device regions
- direct HAL/LL/RTOS/private owner state mutation
- package install bypass
- Platform firmware update apply/commit outside the documented Platform update flow
- simultaneous MSC host ownership and firmware writes to the same FAT volume

Any advanced escape hatch for maintainers must be explicit, logged, gated, and excluded from normal workflows.

---

## CI Use

The CLI should support noninteractive CI for safe workflows.

Allowed CI workflows:

- schema validation
- docs link/contract checks
- deterministic generator checks
- package build/validation
- digital twin package tests where profiles allow
- artifact integrity checks

CI must not:

- flash hardware unless a dedicated hardware runner is configured
- assume simulator or twin evidence is hardware evidence
- auto-approve destructive device operations

---

## Validation Cases

1. `doctor` reports missing host prerequisites and configured tool paths.
2. `generate platform-knobs` refuses to edit generated headers manually and reports generated outputs.
3. package build command refuses to emit installable package when validation fails.
4. firmware build command records build profile, firmware commit, and output paths.
5. flash command prints target and image summary before programming.
6. CDC package upload refuses to run unless developer CDC personality is active.
7. MSC package install workflow does not require PeepShow-specific host tooling for normal copy path.
8. platform-update commands treat update artifacts as distinct from `PeepPkg` packages.
9. platform-update upload over CDC refuses raw flash programming.
10. `platform-knob` command rejects non-live-safe Platform knob.
11. telemetry capture command records schema versions and source profile.
12. trace snapshot command records trace profile and artifact path.
13. twin command refuses hardware-derived profile before required validation evidence exists.
14. evidence command produces tracker-ready metadata without marking completion automatically.

---

## Rule

The CLI reduces manual sequencing.

It does not replace contracts, validation, evidence, or owner boundaries.
