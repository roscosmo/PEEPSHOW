# Runtime Host Contract

This document defines platform-provided runtime hosts and their lifecycle API.

---

## Hosts And Package Scenes

System hosts:

- `SHELL`: built-in OS shell
- `PACKAGE`: mounted package and active scene dispatcher
- `INSTALLER`: package staging and install workflow

The `PACKAGE` host executes the canonical scene types from
[[Scene_Runtime_and_Interaction_Model]]:

- `STATE_SCENE / REACTIVE`
- `SEQUENCE_SCENE / REALTIME`
- `PROGRAM_SCENE / REALTIME`

Hosts own lifecycle. Scene transitions occur inside the mounted package and do
not remount it.

FW0 currently implements a minimal `thRuntime` scaffold for OS lifecycle
visibility. The canonical host tracks the system host, active scene type and ID, execution semantic, lifecycle
state, shell/installer return context, and bounded event counts. HW6 evidence
`EV-HW6-20260813-P1-RUNTIME-044` validates the first shell/installer path:
normal boot enters `SHELL / REACTIVE / RUNNING`; package transfer enters
`INSTALLER / REACTIVE / RUNNING`; the valid-package prompt remains in
`INSTALLER`; and install-stub completion returns to `SHELL` without installer
error. This proves runtime naming and handoff plumbing only. It does not prove
final package execution, PeepPkg install commit, realtime admission, suspend/
resume, or measured power behavior. Its `LP_GRAPH`, `LP_MODULE`, and `RT_SCENE`
probe values predate the canonical scene model and are retained only as
historical evidence until the scaffold is migrated.

HW6 evidence `EV-HW6-20260813-P1-RUNTIMECLOCK-045` adds the first OS clock-intent plumbing proof for `thRuntime`: each admitted runtime command publishes a bounded `REACTIVE_TRANSACTION_ACTIVE` requester update through `thPower`, then releases the runtime requester slot when the command returns. The validated boot capture showed runtime clock request/release `1/1`, reactive/release statuses `0x0/0x0`, requester cap `RT=0x0` after idle settle, and `STOP2 ready=1`. This remains Platform plumbing only; runtime hosts still cannot choose clocks, voltage scale, PLLs, or sleep mode directly.

FW0 probe version 22 adds the first system-action admission use of runtime suspend. When a package runtime class is active and shell policy starts a system overlay action such as MSC entry, the admission layer requests `suspend` from `thRuntime` and waits for a bounded owner ACK before the system action continues. This is still a scaffold: it proves routing, ACK timing, and state recording, not final package suspend handlers, retained package state, or final resume UX. HW6 evidence `EV-HW6-20260814-P1-ADMISSION-053` validates the dry-run path: reactive package stub entered `LP_MODULE / REACTIVE / RUNNING`; MSC-enter admission reported action/result/reason/status `1 / 2 / 3 / 0x0`, counts `request/allow/deny/suspend = 1 / 1 / 0 / 1`, and `thRuntime` moved to `SUSPENDED` with suspend count `1` and zero runtime queue errors.

FW0 probe version 23 uses the same admission shape for power-owned shutdown preparation. START-shutdown, battery-critical, and boot-low-battery prep are system actions owned by `thPower`, not package input, and they may suspend an active package runtime before physical owner quiesce begins. START-cancel is the only validated automatic resume path and only resumes a runtime that was suspended by START-shutdown admission. HW6 evidence `EV-HW6-20260814-P1-POWERADMIT-054` validates the scaffold: START-shutdown admission suspended a reactive `LP_MODULE` runtime (`action/result/reason/status = 4/2/3/0x0`, lifecycle `SUSPENDED`), then START-cancel resumed it (`resume reason/status = 1/0x0`, lifecycle `RUNNING`, suspend/resume `1/1`). This still does not prove final package callbacks, retained package state, save policy, or user-facing shutdown UX.

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
| `DEVICE_INACTIVE` | PeepOS has settled the declared inactive scene route/presentation and suppressed package focus |
| `DEVICE_ACTIVE` | PeepOS has consumed an admitted activation gesture and restored the admitted scene/focus state |

Rules:

- these are lifecycle events, not input actions
- they are emitted only for packages using `TIMEOUT`; `CONTINUOUS` packages remain active and receive neither event
- the physical activation button or chord is never replayed into the package action stream
- `DEVICE_ACTIVE` is the first package-visible event after wake/resume and focus restoration; later physical inputs follow normal routing
- `DEVICE_INACTIVE` is delivered to the resulting mounted package state after its inactive route and presentation are stable
- when the inactive route exits to shell, the package follows normal suspend/stop/unmount ordering and the shell receives the resulting system lifecycle state
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
    uint32_t entry_scene_id;
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

Hosts may dispatch package-visible events, evaluate bounded state/action tables,
run validated sequence timelines, and run approved programmable frame ticks for
the active scene.

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
- transition to another declared package scene through the package scene manager

Hosts may not:
- touch HAL handles directly
- change clocks or sleep mode directly
- mount/unmount storage volumes directly
- expose RTOS threads, queues, timers, or interrupts to packages
- transition to undeclared package scenes directly
- store package asset chunk offsets or storage addresses directly
- consume raw GPIO, EXTI, timer, I2C, joystick register, or debounce state directly

---

## Reactive Host Yield

The `PACKAGE` host executes `STATE_SCENE` work as bounded reactive
transactions. After event dispatch, state transitions, Engine actions,
rendering, and required owner requests settle, the host publishes its next wait
and presentation timeline and yields to PeepOS.

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
- package interaction mode and, for `TIMEOUT`, its route, overlay style, meaningful activity, and bounded deferrals

Scene transitions must preserve this model. Every realtime scene in a `TIMEOUT`
package must declare an inactivity route to a `STATE_SCENE` or shell. Reactive state scenes yield
immediately after each bounded event transaction settles; they do not remain
awake waiting for input.

`SEQUENCE_SCENE` and `PROGRAM_SCENE` have no fixed maximum active duration at
this contract level. They must declare suspend/resume behavior. In a `TIMEOUT`
package they also declare meaningful-activity sources, bounded inactivity
deferral where needed, and an inactive route. Inactivity terminates or suspends
realtime execution before the declared `STATE_SCENE` or shell route is
established.

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
