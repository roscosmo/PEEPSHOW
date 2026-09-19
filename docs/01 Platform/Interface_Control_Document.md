# Interface Control Document (ICD)

This document defines cross-thread and cross-layer message interfaces.

Goal: prevent hidden coupling and ad hoc payload drift.

---

## Message Design Rules

- Fixed-size structs only.
- Explicit enum for message type.
- Version field for forward compatibility.
- No pointers to temporary memory.
- No function pointers in payloads.
- Every request has ownership and timeout semantics.

---

## Queue Registry Template

For each queue define:
- queue name
- producer(s)
- consumer
- message struct
- max depth
- timeout behavior
- overflow behavior

Example:

```text
Queue: qDisplayCmd
Producers: thUI, runtime manager
Consumer: thDisplay
Payload: ps_display_cmd_t v1
Depth: KNOB_Q_DISPLAY_DEPTH
Timeout: bounded, non-infinite
Overflow: drop oldest + emit fault event
```

New active-target communication work must define `qCommCmd` before BLE implementation begins.

---

## Event Flag Registry Template

For each event group define:
- owner thread
- bit assignments
- legal setters
- legal waiters
- clear policy

Bit assignments must live in one header per event group.

---

## API Surface Boundaries

`Platform -> Engine`
- typed lifecycle, rendering, input, audio, storage, and intent APIs only

`Engine -> package/content layer`
- package metadata, assets, save APIs, and runtime-safe content APIs only

`Platform shell/services -> Engine`
- event, focus, rendering, and lifecycle abstractions only

No reverse dependency from Platform owners into Engine, package, or Reference Game modules.

---

## Change Control

### HW6 Calendar Bench Envelope V1

The explicit calendar runtime bench uses existing thPower/thRuntime queues;
no queue sizes or objects change. `ps_calendar_message_t` is compile-time checked
as four 32-bit words: `CAL1` magic/version (0x43414c31), operation, sequence,
registration. No pointers or callbacks are sent.

Power sends NOTIFY=1 and GRANT=3. Runtime sends CLAIM=2 and completion
ACK_APPLIED=4 / ACK_IGNORED=5 / ACK_FAILED=6. Sequence identifies the retained
power-owned occurrence; registration identifies the scene/binding lifetime.
The full identity is retained by the endpoint, not truncated into a pointer.

Control operations: REGISTER=7 uses sequence=registration and registration=scene
activation to identify one runtime-owned immutable intent lease. Runtime cannot
rewrite that fixed POD lease until the matching REGISTERED=8 reply, whose
sequence is setup status and registration is the request identity. CANCEL=9
removes the matching registration. REJECT=10 terminates a stale claim attempt.
DEFER=11 returns an unexecuted grant to pending when runtime suspended; a changed
clock generation or cancelled owner invalidates it instead. Identity counters
do not wrap. The scene activation is unique across live installed replacements
and serves as session/scene identity in this single-scene bench.

Every send is TX_NO_WAIT. A failed send retains the outbound message; each
existing owner service pass attempts at most one send, counted on failure.
There is no synchronous wait, new polling timer, queue resend after success,
or timeout that releases a still-live lease. A missing owner therefore remains
an explicit pending transaction, not permission to overwrite its data or replay
an action. Battery fault handling cancels scheduling and removes its sleep hold;
this bench is not new broken-owner recovery policy. A full queue can delay work
but cannot cause a tight zero-wait receive loop. Native queue stubs exercise
this lifecycle; hardware dispatch remains pending validation.

Clock changes serialize with claims in thPower. Runtime rechecks scene lifetime
and suspension at grant receipt. Completions are retained until enqueued, with
the executed sequence recorded before sending, preventing duplicate execution.
Cancellation, new setup and completion share each producer's existing FIFO.
This envelope does not advertise a public calendar package event encoding.

Any ICD change requires:
1. schema version bump
2. docs update
3. compile-time compatibility checks
4. regression test update
