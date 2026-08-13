# Runtime Host Contract

This document defines platform-provided runtime hosts and their lifecycle API.

---

## Runtime Classes

- `SHELL`: built-in OS shell
- `LP_GRAPH`: reactive event/state transaction runtime
- `LP_MODULE`: Engine-hosted reactive module with a predefined bounded transaction shape
- `RT_SCENE`: frame-paced realtime runtime for richer scenes
- `INSTALLER`: package staging and install workflow

Runtimes are hosts, not game engines embedded in platform core.

FW0 currently implements a minimal `thRuntime` scaffold for OS lifecycle
visibility. It tracks the active runtime class, execution semantic, lifecycle
state, shell/installer return context, and bounded event counts. HW6 evidence
`EV-HW6-20260813-P1-RUNTIME-044` validates the first shell/installer path:
normal boot enters `SHELL / REACTIVE / RUNNING`; package transfer enters
`INSTALLER / REACTIVE / RUNNING`; the valid-package prompt remains in
`INSTALLER`; and install-stub completion returns to `SHELL` without installer
error. This proves runtime naming and handoff plumbing only. It does not prove
final package execution, PeepPkg install commit, realtime admission, suspend/
resume, or measured power behavior.

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

### Symbolic System Lifecycle Events

Lifecycle methods describe host ownership transitions. A mounted runtime may also receive ordered symbolic system lifecycle events:

| Event | Meaning |
|---|---|
| `DEVICE_LOCKED` | PeepOS has suppressed package focus and completed the package's declared lock route |
| `DEVICE_UNLOCKED` | PeepOS has consumed the Start unlock action and restored the admitted runtime/focus state |

Rules:

- these are lifecycle events, not input actions
- the physical Start press used for unlock is never replayed into the package action stream
- `DEVICE_UNLOCKED` is the first package-visible event after wake/resume and focus restoration; later physical inputs follow normal routing
- `DEVICE_LOCKED` is delivered to the resulting mounted package state when the lock route preserves or transitions package state
- when the lock route exits to shell, the package follows normal suspend/stop/unmount ordering and the shell receives the resulting system lifecycle state
- event delivery is bounded and deterministic for a fixed trace

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
    uint32_t runtime_unit_id;
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

## Runtime Logic Execution

Runtime hosts execute validated package logic through [[Runtime_Logic_State_API_Contract]].

Hosts may dispatch package-visible events, evaluate bounded state/action tables, and run approved realtime frame ticks for the active runtime unit.

Runtime logic execution must not expose RTOS threads, queues, timers, interrupts, or Platform hardware APIs to packages.

---

## Host To Platform Requests

Hosts may request:
- present scene or frame updates
- input focus scope activation through [[Input_Focus_API_Contract]]
- symbolic audio cue playback through [[Audio_API_Contract]]
- wake, lifecycle, timer, cadence, and power-intent hints through [[Time_And_Power_Intent_API_Contract]]
- package asset reads/views through [[Package_Asset_Loading_API_Contract]]
- communication sessions and bounded messages through [[Communication_API_Contract]]
- bounded package diagnostics through [[Diagnostics_API_Contract]]
- transition to another declared runtime unit through the runtime manager

Hosts may not:
- touch HAL handles directly
- change clocks or sleep mode directly
- mount/unmount storage volumes directly
- expose RTOS threads, queues, timers, or interrupts to packages
- transition to undeclared runtime units directly
- store package asset chunk offsets or storage addresses directly
- consume raw GPIO, EXTI, timer, I2C, joystick register, or debounce state directly

---

## Reactive Host Yield

`LP_GRAPH` and `LP_MODULE` hosts execute bounded reactive transactions. After event dispatch, state transitions, Engine actions, rendering, and required owner requests settle, the host publishes its next reactive wait contract and yields to PeepOS.

The host remains mounted and retains its declared state while the CPU sleeps. A wake does not remount the package; it resumes lifecycle ordering, delivers the admitted symbolic event, executes the next transaction, and yields again.

A waiting visual may continue through a Platform autonomous backend while the host is yielded. No package code runs during that playback.

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

Detailed package-facing rules are defined in [[Time_And_Power_Intent_API_Contract]].

Runtime expresses intent only:

- reactive wait contract and event interests
- waiting-visual preference and fallback
- realtime cadence and frame budget where applicable
- symbolic wake intents
- latency tolerance
- declared meaningful-activity sources
- optional input-lock policy and bounded deferrals

Runtime unit transitions must preserve this model. A realtime unit must return to a declared reactive unit or shell/system route according to its package manifest and power policy. Reactive hosts yield immediately after each bounded event transaction settles; they do not remain awake waiting for input.

`RT_SCENE` has no fixed maximum active duration at this contract level. It must declare meaningful-activity sources, suspend/resume behavior, bounded lock deferral where needed, and a reactive fallback. If the package enables automatic input locking, lock activation terminates or suspends realtime execution before the declared lock route is established.

Bounded interactive peer-wait treatment is admitted only through [[Communication_API_Contract]] and target-profile policy. Runtime hosts must not treat an active communication session as meaningful local activity, an unlimited lock deferral, or a stay-awake grant.

Power manager maps intent to hardware policy.

---

## Host Compliance Checklist

A runtime host is compliant only if:
1. lifecycle methods are complete and bounded
2. no forbidden direct hardware access exists
3. suspend/resume tests pass
4. power intent is explicit
5. install/update interactions are safe
