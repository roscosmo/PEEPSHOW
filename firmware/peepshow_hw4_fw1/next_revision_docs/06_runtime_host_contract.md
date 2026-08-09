# Runtime Host Contract

This document defines platform-provided runtime hosts and their lifecycle API.

---

## Runtime Classes

- `SHELL`: built-in OS shell
- `LP_GRAPH`: low-power event/state driven runtime
- `LP_TEMPLATE`: hosted template modules/minigames
- `RT_SCENE`: higher activity runtime for richer scenes
- `INSTALLER`: package staging and install workflow

Runtimes are hosts, not game engines embedded in platform core.

---

## Lifecycle Contract

All runtimes must implement:
1. `mount`
2. `start`
3. `suspend`
4. `resume`
5. `stop`
6. `unmount`

No runtime switch is allowed without orderly lifecycle completion.

---

## Suggested Interface (C-Level)

```c
typedef enum {
    HOST_OK = 0,
    HOST_ERR_INVALID_STATE,
    HOST_ERR_DEPENDENCY,
    HOST_ERR_RESOURCE,
    HOST_ERR_INTERNAL
} host_result_t;

typedef struct {
    uint32_t package_id;
    uint32_t runtime_flags;
    const void *manifest;
} host_mount_args_t;

typedef struct {
    host_result_t (*mount)(const host_mount_args_t *args);
    host_result_t (*start)(void);
    host_result_t (*suspend)(void);
    host_result_t (*resume)(void);
    host_result_t (*stop)(void);
    host_result_t (*unmount)(void);
    void (*tick)(uint32_t now_ms);
} host_vtable_t;
```

---

## Host To Platform Requests

Hosts may request:
- present/invalidate regions
- audio cue playback
- wake intent hints
- timer cadence hints
- storage reads through package API
- public knob changes through validated public-knob API

Hosts may not:
- touch HAL handles directly
- change clocks or sleep mode directly
- mount/unmount storage volumes directly
- set private knobs
- bypass owner-thread routing when requesting public knob changes

---

## Suspend/Resume Rules

- `suspend` must be bounded and idempotent.
- `resume` must revalidate dependencies and fail cleanly.
- Host state needed for resume must use explicit retained-state contracts.
- Resume failure must route to shell with a user-visible error.

---

## Failure Handling

Runtime failures must map to one of:
- recover in-place
- safe stop and return to shell
- force runtime unmount and package quarantine

Do not leave runtime manager in partial state.

---

## Power Intent Interface

Runtime expresses intent only:
- desired update cadence
- wake sources needed
- tolerance for latency
- active/idle hints

Power manager maps intent to hardware policy.

---

## Public Knob Access Rules

Runtime hosts may request only knobs marked `public` in knob schema.

Public knob requests must:
- include runtime class identity
- pass bounds and mode checks
- be routed to the knob owner thread for application

Rejected requests must return explicit error codes and emit bounded diagnostics.

---

## Host Compliance Checklist

A runtime host is compliant only if:
1. lifecycle methods are complete and bounded
2. no forbidden direct hardware access exists
3. suspend/resume tests pass
4. power intent is explicit
5. install/update interactions are safe
6. public-knob requests use validated API path only
