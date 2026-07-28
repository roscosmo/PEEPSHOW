# File Ownership

This note will track module ownership once firmware files exist.

For each module, record:

- owner layer
- owning subsystem
- allowed dependencies
- forbidden dependencies
- required tests
- related authority notes

## Template

| Path | Owner | Allowed Dependencies | Forbidden Dependencies | Required Tests | Notes |
|---|---|---|---|---|---|
| `firmware/platform/src/owners/display/*` | Platform display owner | Platform RTOS, Platform power, HAL display path | Engine, Reference Game | display state-machine tests | |

Related:

- [[Repository_Structure]]
- [[Architecture_and_Boundaries]]
- [[Authority_and_Invariants]]
