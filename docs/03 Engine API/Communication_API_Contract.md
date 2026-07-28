# Communication API Contract

This document defines the Engine-facing communication API for PeepOS packages and game-development tools.

The communication API exposes abstract sessions and bounded messages. It does not expose BLE, NINA, UART, flow control, GATT, GAP, advertising intervals, pairing commands, bonding storage, module straps, or hardware wake behavior.

Related:

- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[BLE_Communication_Contract]]
- [[Communication_Index]]
- [[Power_and_Sleep_Policy]]
- [[Brought_Up_Tracker]]

---

## Purpose

Communication is a creative package primitive for multiplayer, companion-app, synchronization, and experimental connected behaviors.

Packages work with:

- communication contexts
- sessions
- peers
- bounded schema messages
- session lifecycle events
- companion-app message channels

The Platform owns the physical communication service. For HW6, the known hardware path is BLE/NINA over `LPUART1`, but packages must not depend on that implementation.

NINA driver details and final throughput/latency limits are bring-up dependent and must be refined after measured HW6 evidence exists.

---

## Ownership Boundary

The Platform owns:

- BLE/NINA module control
- UART and flow-control behavior
- module reset and strap pins
- BLE command protocol
- pairing/bonding
- bonding record storage
- communication power state
- module fault recovery
- sleep and quiesce coordination

The Engine owns:

- package communication capability declarations
- communication context admission
- session lifecycle API
- peer/event routing
- message schema validation
- message size/rate/order limits
- digital twin communication simulation contract
- package-facing behavior when communication is unavailable, disconnected, or timed out

Packages own:

- session role intent
- package message schema
- package session state logic
- gameplay handling of peer/session events
- session-required or fallback routing decisions

Packages do not own BLE hardware behavior.

---

## Core Rules

- Packages use abstract communication sessions and bounded messages.
- Packages must not control BLE/NINA hardware, UART, pins, module commands, pairing, bonding, or bonding storage.
- Packages must not depend on a specific BLE profile, advertising interval, GATT characteristic, UART baud rate, or module command set.
- Message schemas must be versioned and bounded.
- Message send/receive behavior must have fixed maximum payload size and rate limits.
- Communication runtime units must declare either fallback/route behavior or session-required admission behavior.
- Multiplayer-only and companion-required package experiences are valid when declared and bounded.
- Interactive session waits may request only the bounded peer-wait behavior granted by the target profile.
- BLE hardware faults are Platform health events, not package gameplay logic.
- Peer disconnects, session closes, message timeouts, and session admission failures are package-visible session events.
- For HW6, BLE does not wake the device unless a future measured profile explicitly grants that capability.

---

## HW6 BLE Boundary

HW6 BLE behavior is currently defined by [[BLE_Communication_Contract]].

Package-facing implications:

- communication is an awake-session feature on HW6.
- `LP_GRAPH` cannot depend on receiving BLE messages.
- packages must not declare BLE-message wake on HW6 profiles.
- Platform may power the module down when no approved communication context is active.
- final message payload limits, session limits, latency, and throughput must be updated after NINA bring-up evidence.

Future hardware profiles may add communication wake or different transports. Package-facing APIs should remain transport-agnostic.

---

## Capability Model

Canonical names live in [[PeepOS_Capability_Registry]].

Communication capabilities:

| Capability | Meaning |
|---|---|
| `comm.multiplayer` | package may use abstract multiplayer sessions and bounded messages |
| `comm.companion` | package may use companion-app sessions and bounded messages |
| `comm.local_loopback` | host/digital-twin or diagnostic loopback session support |
| `comm.session_required` | runtime unit may require an active communication session for admission |
| `comm.message_schema` | package declares bounded versioned message schemas |

Capability names must not include BLE, NINA, UART, GAP, GATT, SPS, AT commands, pins, or bonding storage.

---

## Communication Profile Schema

Packages declare communication use as package data.

Conceptual schema:

```text
communication_profile:
  contexts[]:
    context_id
    runtime_unit_refs[]
    mode
    role_intent
    session_type
    max_peers
    message_schema_ref
    rate_limits
    timeout_policy
    interactive_wait_policy
    ordering_policy
    session_end_route
    fallback_route
    diagnostic_label
```

Modes:

| Mode | Meaning |
|---|---|
| `none` | runtime unit does not use communication |
| `optional` | runtime unit can use communication but has a declared non-session route |
| `session_required` | runtime unit requires an active session before entry or meaningful operation |

Role intents:

| Role | Meaning |
|---|---|
| `host` | package prefers to create/advertise a session |
| `joiner` | package prefers to join an existing session |
| `either` | package can host or join |
| `companion_client` | package communicates with a companion app/session |

Rules:

- every communication context must belong to a declared runtime unit.
- every context must reference a message schema unless it uses only built-in session lifecycle events.
- session-required runtime units must declare an admission route when no session exists.
- optional contexts must declare fallback/route behavior.
- timeout and session-end policy must be explicit.
- interactive wait policy must be explicit when a context expects the local device to remain awake while a remote peer is taking a turn or supplying the next meaningful session action.
- no context may declare communication wake on HW6 profiles.

---

## Interactive Session Wait Policy

Interactive communication can include bounded peer-wait periods where the local player has finished an action and is waiting for the next meaningful remote action.

This is a declared communication policy, not a package-owned power override.

Conceptual shape:

```text
interactive_wait_policy:
  requested
  refresh_message_types[]
  wait_expiry_route
```

Rules:

- target profiles decide whether interactive session wait is supported and the maximum awake grace that may be granted.
- packages may request peer-wait behavior only through a declared communication context.
- package code does not set the PeepOS input-lock timeout or create an unbounded peer wait.
- `refresh_message_types[]` may name bounded schema messages that represent meaningful peer progress or input.
- generic keepalive traffic, presence chatter, and arbitrary high-rate messages must not be used to defeat low-power policy.
- wait expiry must route through a declared session idle, pause, fallback, or reactive transition.
- remote activity can refresh an admitted peer-wait grace only where the target profile grants that behavior.
- HW6 peer-wait policy does not create BLE wake; after the reactive host yields and hardware sleeps, communication cannot wake the package.

This allows experiences such as turn-based multiplayer to keep the watching player responsive for a bounded peer turn without making an active session an unlimited awake lease.

---

## Session API

Package-facing operations:

```text
comm.advertise_session(context_id)
comm.join_session(context_id, session_ref)
comm.leave_session(reason)
comm.session_state(context_id)
comm.peer_list(context_id)
```

Package-visible events:

```text
session_advertising(context_id)
session_joined(context_id)
session_join_failed(context_id, reason)
session_closed(context_id, reason)
peer_joined(peer_id)
peer_left(peer_id, reason)
session_timeout(context_id)
```

Rules:

- session references are abstract PeepOS handles, not BLE addresses.
- peer IDs are package/session-scoped identities, not raw BLE addresses.
- Platform pairing/bonding remains outside package control.
- session events are valid gameplay events.
- hardware/module faults are diagnostics and Platform health events.

---

## Message API

Package-facing messages are schema-bound.

```text
comm.send(context_id, peer_or_group, message_type, payload)
comm.receive(context_id)
```

Message schema:

```text
message_schema:
  schema_id
  schema_version
  max_message_bytes
  message_types[]:
    message_type
    payload_schema
    max_rate_hz
    reliability_class
    ordering_policy
  compatibility_policy
```

Rules:

- messages are bounded and versioned.
- unknown message types are rejected or ignored according to schema policy.
- payloads must validate before send and after receive.
- packages do not send arbitrary byte streams in v1.
- message processing cost must be bounded.
- message queues must be bounded.
- rate limits must be validated before package export.

Reliability and ordering classes are contract-level behavior. They must not promise BLE-level details until measured and mapped into the target profile.

---

## Companion App Semantics

Companion behavior is separate from generic multiplayer even when both use the same Platform BLE service.

Companion package primitives:

```text
companion.session_state()
companion.send_message(message_type, payload)
companion.receive_message()
```

Rules:

- pairing/bonding remains Platform-owned.
- companion identity is abstract.
- companion messages use package-declared schemas.
- companion availability may be represented as a package/session state.
- companion data must not mutate Platform settings, bonding, install metadata, or protected storage except through approved Platform/system flows.

---

## Runtime Class Rules

| Runtime Class | Communication Behavior |
|---|---|
| `LP_GRAPH` | no active BLE receive dependency on HW6; local/offline graph behavior only |
| `LP_MODULE` | optional or session-required contexts allowed while awake if target profile grants capability |
| `RT_SCENE` | multiplayer contexts allowed only within declared realtime budget and rate limits |

Waiting visuals do not run arbitrary package communication logic.

For HW6, communication cannot be a wake source. A communication session may raise the Platform power floor while admitted, but the reactive host still yields when its bounded transaction settles and Platform remains the sleep authority.

An admitted interactive wait policy may grant bounded peer-wait behavior when the target profile supports it. It cannot bypass an enabled input lock, create unbounded lock deferral, or turn message traffic into a communication wake source.

---

## Tool-Time Validation

Tooling must validate communication before package compilation/export.

Reject:

- BLE, NINA, UART, GAP, GATT, SPS, AT command, pin, bonding storage, or flow-control references.
- communication wake intent on HW6 profiles.
- unbounded message size.
- unbounded message queue.
- unbounded send rate.
- unknown or unresolved message schema.
- session-required runtime unit without admission/session route.
- optional communication context without fallback/route behavior.
- interactive wait policy when the selected target profile does not support it.
- interactive wait policy without a declared wait-expiry route.
- interactive wait policy that depends on keepalive or unbounded chatter to stay awake.
- `LP_GRAPH` unit depending on BLE receive.
- realtime communication context without declared budget/rate limits.
- arbitrary byte stream unless a future bounded stream primitive is contracted.

Authoring tools should explain failures in PeepOS terms, such as:

```text
This reactive graph waits for a communication message, but HW6 communication cannot wake the device.
Use an awake session module or provide a local route.
```

They should not expose BLE/NINA implementation terms to normal game authors.

---

## Runtime Fault And Disconnect Handling

Package-visible communication events:

- peer joined
- peer left
- session joined
- session closed
- join failed
- message timeout
- message schema mismatch
- session timeout

Platform/Engine diagnostic events:

- BLE module boot timeout
- UART framing fault
- module command timeout
- pairing/bonding storage failure
- NINA reset/config failure
- communication owner fault

Packages may handle session events as gameplay. Packages must not handle low-level hardware/module faults as normal gameplay branches.

If the communication owner faults, Platform/Engine records diagnostics and applies context/session lifecycle policy.

---

## Digital Twin Requirements

The digital twin must use the same communication contract as the hardware runtime.

The twin may simulate:

- loopback sessions
- multi-instance host/join sessions
- companion sessions
- peer join/leave
- bounded message send/receive
- delayed messages
- dropped messages
- reordered messages where the schema policy allows it
- disconnects and timeouts
- communication fault injection

Rules:

- BLE/NINA commands are not exposed.
- bonding and pairing internals are not modeled as hardware behavior.
- message schema, payload bounds, rate limits, and session-required admission behavior must match package contracts.
- interactive peer-wait behavior must enforce target-profile grace limits, refresh policy, and expiry routes.
- twin profiles must model HW6's no-BLE-wake rule unless a future target profile grants communication wake.
- twin communication evidence is not HW6 BLE bring-up evidence.

---

## Validation Cases

1. package message schema validates max payload size, message types, and rate limits.
2. package cannot reference BLE/NINA/UART/GATT/GAP/bonding details.
3. `LP_GRAPH` communication receive dependency fails HW6 validation.
4. communication wake intent fails HW6 validation.
5. session-required runtime unit stays in declared admission route when no session exists.
6. optional communication runtime unit follows declared fallback/route behavior.
7. peer disconnect is delivered as package-visible session event.
8. BLE owner fault is logged as Platform/Engine diagnostic and not exposed as UART/NINA error to package gameplay code.
9. digital twin multi-instance session replay is deterministic for a fixed trace.
10. digital twin delayed/drop/disconnect fault injection validates package session behavior without acting as HW6 bring-up evidence.
11. interactive peer-wait policy validates only when the target profile grants it and the context declares a wait-expiry route.
12. keepalive or unbounded chatter cannot refresh peer-wait grace, prevent reactive yield, or bypass enabled input lock.
