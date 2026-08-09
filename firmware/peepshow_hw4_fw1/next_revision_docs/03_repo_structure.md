# Repository Structure

This is the recommended layout for the clean new project.

---

## Top-Level Layout

```text
/
  CMakeLists.txt
  CMakePresets.json
  PeepOS_<rev>.ioc
  README.md
  docs/
  config/
  tools/
  Core/
  Drivers/
  Middlewares/
  cmake/
  platform/
  runtime_hosts/
  ui_services/
  package_sdk/
  tests/
```

---

## Directory Purpose

`docs/`
- Authoritative project docs (architecture, contracts, bring-up, test plans)

`config/`
- Compile-time knobs and schemas
- Memory budgets and static resource configs

`tools/`
- Code generation and validation scripts
- Package schema validators

`Core/`, `Drivers/`, `Middlewares/`, `cmake/stm32cubemx/`
- CubeMX-generated and vendor layers
- Do not modify generated code outside allowed user blocks

`platform/`
- Owner-thread implementations and platform services only
- No package-specific logic

`runtime_hosts/`
- Host implementations (`shell`, `lp_graph`, `lp_template`, `rt_scene`, `installer`)
- Host adapters only, no direct peripheral ownership

`ui_services/`
- Shared UX primitives reusable by shell and runtimes

`package_sdk/`
- Public package schema, validators, and host-facing API docs
- generated public knob contract artifacts and IDs for package/runtime use

`tests/`
- Host lifecycle tests, parser tests, power-policy tests, integration harnesses

---

## Recommended Internal Boundaries

`platform/include/`
- public platform service headers

`platform/src/owners/`
- thread owners for display/audio/input/sensor/storage/power

`platform/src/services/`
- non-owner logic that routes requests and enforces policy

`runtime_hosts/include/`
- host API and host lifecycle headers

`runtime_hosts/src/<host_name>/`
- one folder per host, no shared mutable globals across hosts

---

## Naming Rules

- Prefix platform-owned APIs with `ps_`.
- Prefix runtime-host interfaces with `host_`.
- Prefix package-facing contracts with `pkg_`.
- Use explicit enum names for all state machines.

---

## File-Level Ownership Manifest

Maintain one ownership file:
- `docs/file_ownership.md`

For each module:
- owner
- allowed dependencies
- forbidden dependencies
- required tests
