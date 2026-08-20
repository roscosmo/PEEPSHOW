# BLE Communication Contract

This document defines the retained HW6 BLE/NINA module boundary and required state machine. HW5 results are sequencing baselines only.

BLE is a Platform communication service. It exposes generic communication capabilities to the Engine, not game-specific radio behavior.

Engine and Reference Game code must not own the UART, module reset pins, strap pins, flow-control pins, persistent bonding data, or BLE command protocol directly.

## Hardware Path

Module: `NINA-B112-04B`.

Transport:

- `LPUART1`
- hardware RTS/CTS flow control
- exact module command/profile behavior is a bring-up item

| Signal | MCU Pin | CubeMX Signal | Owner |
|---|---|---|---|
| UART TX | `PA2` | `LPUART1_TX` | `thComm` |
| UART RX | `PA3` | `LPUART1_RX` | `thComm` |
| UART RTS | `PB12` | `LPUART1_RTS` | `thComm` |
| UART CTS | `PB13` | `LPUART1_CTS` | `thComm` |
| Mode / strap | `PC4` | `NINA_SW1` | `thComm` |
| Mode / strap | `PC5` | `NINA_SW2` | `thComm` |
| Reset | `PC6` | `NINA_NRST` | `thComm` |
| DTR | `PC7` | `NINA_DTR` | `thComm` |
| DSR | `PC8` | `NINA_DSR` | `thComm` |

No BLE wake IRQ is assigned in the HW6 full-intent IOC, and HW6 grants no BLE wake capability pending contrary measured evidence.

NINA `RESET_N` is active low. Firmware safe default is reset asserted low until `thComm` intentionally brings the module online.

NINA system-control pins are configuration-sensitive. `NINA_SW1`, `NINA_SW2`, `NINA_DTR`, and `NINA_DSR` should be CubeMX-configured as high-Z input or analog/no-pull by default unless a validated bring-up sequence explicitly needs output drive.

## NINA Control Signal Notes

The following notes come from the NINA/u-connectXpress control-signal summary used for the retained module design.

System control inputs:

- `RESET_N` resets the module and is active low.
- `SWITCH_2` driven low during startup restores UART serial settings to default values.
- `SWITCH_2` can be used to open a Bluetooth LE connection with a peripheral device.
- `SWITCH_1` and `SWITCH_2` driven low during startup can enter the bootloader.
- `SWITCH_1` and `SWITCH_2` driven low during startup and held low for 10 seconds can exit bootloader mode and restore factory defaults.

UART-side auxiliary pins:

- `DSR` can be configured to enter command mode, disconnect or toggle connectable status, enable/disable the rest of the UART interface, or enter/wake from sleep.
- `DTR` can be configured to indicate system mode, SPS peer connection, bonded BLE device connection, or Bluetooth LE GAP connection.

HW6 policy:

- do not drive `NINA_SW1` or `NINA_SW2` low during module reset release unless intentionally entering a documented NINA startup mode
- do not use `NINA_DSR` or `NINA_DTR` until direction, polarity, and configured function are documented in the BLE bring-up evidence
- `fw1` reference `.ioc` assigns NINA auxiliary GPIO to `GPIO_Analog` high-Z/no-pull defaults; preserve equivalent safe behavior in `fw0` before generated firmware is allowed to release `NINA_NRST`

## Ownership

- `thComm` is the sole owner of `LPUART1` and BLE control GPIO.
- Other threads submit communication requests through `qCommCmd` or approved Engine communication APIs.
- `thComm` owns module reset, strap sequencing, command framing, timeouts, retries, pairing state, and health publication.
- `thStorage` owns persistent storage of pairing/bonding records on request from `thComm`.
- No Engine or Reference Game code may touch `LPUART1`, NINA GPIO, BLE module commands, or bonding storage directly.

## Intended Uses

BLE is primarily for:

- multiplayer
- possible companion app support
- pairing/bonding
- diagnostics where explicitly enabled

BLE is off unless a mode explicitly requests it, such as multiplayer, pairing, companion-app connection, or an approved diagnostic session.

## Current CubeMX Baseline

- `LPUART1` baud rate is `115200`.
- `LPUART1` clock source is HSI at `16 MHz`.
- RTS/CTS pins are assigned.
- First BLE implementation should use interrupt-driven RX/TX static ring buffers. UART DMA is intentionally deferred until throughput or CPU measurements justify it.
- No BLE wake IRQ is currently assigned.
- `NINA_NRST` is active-low reset. `NINA_SW1`, `NINA_SW2`, `NINA_DTR`, and `NINA_DSR` should default high-Z/no-pull until a validated NINA function requires them.

## Required Knobs

The following values are compile-time knobs for FW0 NINA command transport and bring-up timing:

| Knob | Purpose |
|---|---|
| `KNOB_COMM_BLE_RESET_ASSERT_MS` | minimum reset assertion time |
| `KNOB_COMM_BLE_BOOT_WAIT_MS` | maximum wait for module boot-ready evidence |
| `KNOB_COMM_BLE_BOOT_DRAIN_MS` | boot-banner drain window before command probes |
| `KNOB_COMM_BLE_RX_WINDOW_MS` | bounded command-response receive window |
| `KNOB_COMM_BLE_RX_QUIET_MS` | receive quiet period used to end a response |
| `KNOB_COMM_BLE_STOP_SETTLE_MS` | NINA-B1 `AT&D4` / DSR Sleep/System-OFF settling interval |
| `KNOB_COMM_BLE_WAKE_SETTLE_MS` | DSR wake settling interval before commands resume |
| `KNOB_COMM_BLE_RX_BYTE_TIMEOUT_MS` | UART receive byte timeout |
| `KNOB_COMM_BLE_TX_TIMEOUT_MS` | UART transmit timeout |
| `KNOB_COMM_BLE_RX_BUFFER_BYTES` | static command-response RX buffer size |

Advertising interval, connection interval bounds, scan duty cycle, pairing timeout, idle disconnect timeout, radio shutdown timeout, and MTU / packet batching limits remain open until real BLE radio behavior is implemented.

BLE async transfer policy is interrupt-driven RX/TX static ring buffers first. UART DMA must remain disabled unless a later measured throughput or CPU-load requirement justifies it.

## UART Buffering Policy

First BLE bring-up uses interrupt-driven UART servicing with static RX/TX ring buffers.

Rules:

- RX and TX buffers are fixed-size, statically allocated rings.
- IRQ handlers only move bytes and signal `thComm`; protocol parsing remains in `thComm`.
- All command/response waits are bounded by explicit timeouts.
- UART DMA is not part of first bring-up.
- DMA may be reconsidered only after measured throughput or CPU-load evidence shows interrupt-driven rings are insufficient.

## Public Capability Model

The Platform may expose generic capabilities such as:

- BLE available/unavailable
- multiplayer session advertise/join/leave
- companion-app advertise/connect/disconnect
- pairing/bonding status
- bounded message send
- bounded message receive
- diagnostic module info

The Engine consumes capabilities, not hardware commands.

Reference Game code consumes Engine communication APIs only.

## Pairing And Bonding

Pairing/bonding data must be persistent.

Rules:

- bonding records are stored in protected Platform storage, not host-exposed staging storage
- `thComm` requests bonding record reads/writes through `thStorage`
- bonding writes must be power-fail safe
- bonding records must be versioned or invalidatable
- pairing/bonding failure must not corrupt existing valid records
- erase/reset of bonding data must be an explicit Platform command

## State Machine

| State | Meaning |
|---|---|
| `BLE_OFF` | module held in safe inactive state; UART inactive |
| `BLE_RESET_ASSERT` | reset asserted and strap pins driven only as required by validated bring-up policy |
| `BLE_BOOT_WAIT` | reset released; waiting for module boot-ready evidence |
| `BLE_CONFIG` | module configuration commands in progress |
| `BLE_IDLE` | module configured and ready, not advertising or connected |
| `BLE_ADVERTISING` | advertising enabled for multiplayer, pairing, companion app, or diagnostics |
| `BLE_CONNECTED` | link established, no active transfer |
| `BLE_PAIRING` | pairing/bonding exchange active; persistent record update may follow |
| `BLE_TRANSFER` | bounded send/receive operation active |
| `BLE_SUSPENDING` | quiesce sequence in progress |
| `BLE_SUSPENDED` | communication paused for sleep or policy |
| `BLE_ERROR` | module, UART, timeout, framing, pairing, storage, or recovery fault detected |

## FW0 Mode Requests

The FW0 owner state machine exposes explicit BLE mode requests for power-policy integration:

| Mode | FW0 behavior | Status |
|---|---|---|
| `RESET_HELD` | Deinitializes the UART, asserts NINA reset low, and leaves DSR high-Z. This is a hard comparison/fallback state, not the routine low-power policy. | Target-validated |
| `SLEEP_SYSTEM_OFF` | Wakes/configures the NINA command path, applies `AT&D4`, drives DSR high, deinitializes the UART, leaves reset released, and parks in `BLE_SUSPENDED`. This is intended to map to u-connectXpress DSR-controlled Sleep/System OFF. | Target-validated |
| `SEARCHING` | Brings the NINA to owner `BLE_ADVERTISING` as an OS placeholder. Real advertising commands are not implemented yet. | Placeholder |
| `PAIRING` | Brings the NINA to owner `BLE_PAIRING` as an OS placeholder. Real pairing flow is not implemented yet. | Placeholder |
| `CONNECTED` | Brings the NINA to owner `BLE_CONNECTED` as an OS placeholder. Real link/SPS behavior is not implemented yet. | Placeholder |

`SLEEP_SYSTEM_OFF` is the routine NINA-B1 low-power target when BLE radio service is not required. `RESET_HELD` is the reset-asserted hard-off comparison/fallback mode. The old FW0 helper names `STOP` and `SHUTDOWN` are retained only as compatibility aliases for these two physical policies.

STOP2 resident policy preserves the logical active BLE mode while parking the physical module. The baseline STOP2 path targets `SLEEP_SYSTEM_OFF`, including the normal boot-default/off case, so the NINA reaches the validated `AT&D4` / DSR low-power path with reset released, DSR high, and UART deinitialized. This path should allow NFC/GPIO wake while BLE radio wake is unavailable, and NINA wake must be treated as a fresh module startup. `RESET_HELD` is reserved for an explicit reset-held request or bounded fault fallback, and must not be used as the default STOP2 resident state unless measurement proves it is better.

## Events

| Event | Source | Meaning |
|---|---|---|
| `EV_BLE_ENABLE_REQ` | Platform/Engine capability request | bring module online |
| `EV_BLE_DISABLE_REQ` | Platform/power policy | take module offline |
| `EV_BLE_RESET_ASSERTED` | `thComm` timer | reset assertion window elapsed |
| `EV_BLE_BOOT_READY` | `thComm` UART/probe | boot-ready evidence observed |
| `EV_BLE_BOOT_TIMEOUT` | `thComm` timer | module did not become ready |
| `EV_BLE_CONFIG_OK` | `thComm` command response | configuration completed |
| `EV_BLE_CONFIG_FAIL` | `thComm` command response/timeout | configuration failed |
| `EV_BLE_ADV_START_REQ` | Engine capability request | start advertising |
| `EV_BLE_ADV_STOP_REQ` | Engine capability request | stop advertising |
| `EV_BLE_CONNECTED` | module event | connection established |
| `EV_BLE_DISCONNECTED` | module event | connection ended |
| `EV_BLE_PAIRING_START` | module/user event | pairing or bonding started |
| `EV_BLE_PAIRING_DONE` | module event | pairing or bonding completed |
| `EV_BLE_BOND_STORE_OK` | `thStorage` response | bonding record persisted |
| `EV_BLE_BOND_STORE_FAIL` | `thStorage` response | bonding record write failed |
| `EV_BLE_TX_REQ` | Engine capability request | send bounded message |
| `EV_BLE_RX_READY` | module/UART event | received bounded message |
| `EV_BLE_TRANSFER_DONE` | `thComm` | transfer completed |
| `EV_QUIESCE` | `thPower` | prepare for sleep/mode transition |
| `EV_RESUME` | `thPower` | resume from sleep/mode transition |
| `EV_BLE_FAULT` | `thComm` | UART/module/protocol fault |
| `EV_RECOVER_OK` | `thComm` | bounded recovery completed |
| `EV_RECOVER_EXHAUSTED` | `thComm` | retry limit reached |

## Transition Rules

| Current | Event | Next | Required Action |
|---|---|---|---|
| `BLE_OFF` | `EV_BLE_ENABLE_REQ` | `BLE_RESET_ASSERT` | drive only required validated strap pins, assert reset, initialize UART path as needed |
| `BLE_RESET_ASSERT` | `EV_BLE_RESET_ASSERTED` | `BLE_BOOT_WAIT` | release reset, start boot wait timer |
| `BLE_BOOT_WAIT` | `EV_BLE_BOOT_READY` | `BLE_CONFIG` | send bounded configuration sequence |
| `BLE_BOOT_WAIT` | `EV_BLE_BOOT_TIMEOUT` | `BLE_ERROR` | publish boot timeout fault |
| `BLE_CONFIG` | `EV_BLE_CONFIG_OK` | `BLE_IDLE` | publish ready health |
| `BLE_CONFIG` | `EV_BLE_CONFIG_FAIL` | `BLE_ERROR` | publish config fault |
| `BLE_IDLE` | `EV_BLE_ADV_START_REQ` | `BLE_ADVERTISING` | send bounded advertise-start command |
| `BLE_ADVERTISING` | `EV_BLE_ADV_STOP_REQ` | `BLE_IDLE` | send bounded advertise-stop command |
| `BLE_ADVERTISING` | `EV_BLE_CONNECTED` | `BLE_CONNECTED` | publish connection event |
| `BLE_CONNECTED` | `EV_BLE_PAIRING_START` | `BLE_PAIRING` | run bounded pairing/bonding flow |
| `BLE_PAIRING` | `EV_BLE_PAIRING_DONE` | `BLE_PAIRING` | request persistent bond record update if needed |
| `BLE_PAIRING` | `EV_BLE_BOND_STORE_OK` | `BLE_CONNECTED` | publish pairing/bonding complete |
| `BLE_PAIRING` | `EV_BLE_BOND_STORE_FAIL` | `BLE_ERROR` | preserve previous valid record and publish storage fault |
| `BLE_CONNECTED` | `EV_BLE_TX_REQ` | `BLE_TRANSFER` | start bounded transfer |
| `BLE_CONNECTED` | `EV_BLE_RX_READY` | `BLE_TRANSFER` | drain bounded receive frame |
| `BLE_TRANSFER` | `EV_BLE_TRANSFER_DONE` | `BLE_CONNECTED` | publish completion/event |
| `BLE_CONNECTED` | `EV_BLE_DISCONNECTED` | `BLE_IDLE` | clear connection context |
| any online state | `EV_QUIESCE` | `BLE_SUSPENDING` | stop accepting new requests, finish/abort bounded transfer |
| `BLE_SUSPENDING` | `EV_BLE_TRANSFER_DONE` | `BLE_SUSPENDED` | place module/UART in policy-defined idle state |
| `BLE_SUSPENDED` | `EV_RESUME` | `BLE_BOOT_WAIT` | revalidate module readiness |
| any online state | `EV_BLE_DISABLE_REQ` | `BLE_OFF` | stop advertising/connection, assert safe module state |
| any state | `EV_BLE_FAULT` | `BLE_ERROR` | stop new work, publish fault |
| `BLE_ERROR` | `EV_RECOVER_OK` | `BLE_RESET_ASSERT` | retry through reset path |
| `BLE_ERROR` | `EV_RECOVER_EXHAUSTED` | `BLE_OFF` | hold safe state and escalate health fault |

Invalid transitions must be rejected and logged.

## Request Rules

- All send/receive requests must be bounded by static RX/TX ring-buffer limits.
- No request may block indefinitely waiting for BLE response.
- No dynamic allocation is allowed in the communication hot path; RX/TX rings are statically allocated.
- New requests are rejected during `BLE_SUSPENDING`, `BLE_SUSPENDED`, and `BLE_ERROR`.
- Engine-facing APIs report capability status instead of exposing module internals.

## Mode Behavior

| Mode | BLE Policy |
|---|---|
| `SHELL` | off by default; allowed for pairing, settings, diagnostics, or companion-app flow when explicitly requested |
| `STATE_SCENE` | off unless a declared bounded communication transaction or interactive-session wait is admitted |
| `SEQUENCE_SCENE` | off unless a declared bounded communication action can meet the realtime timeline budget |
| `PROGRAM_SCENE` | allowed for multiplayer only when declared capabilities and transfer latency satisfy realtime budgets |
| `INSTALLER` | disabled unless explicitly used for diagnostics; USB/storage ownership remains primary |

BLE does not wake the device in HW6 unless a future target-qualified profile and hardware contract explicitly grant it.

## Power Rules

- BLE defaults off unless multiplayer, companion, pairing, or diagnostics explicitly requests it.
- Module reset is held asserted while BLE is off; strap/control pins are high-Z/no-pull unless required by validated bring-up or operation. If a validated function needs one, `thComm` reconfigures that pin deliberately and restores the safe default afterward.
- `thComm` must quiesce UART activity before STOP entry.
- If BLE is not actively needed, module should be held in the lowest safe state.
- Any active BLE connection must express power intent to `thPower`; `thPower` remains the authority on sleep depth.

## Failure Policy

BLE failure is non-fatal.

On BLE fault:

- stop accepting communication requests
- preserve existing valid bonding records
- publish communication health fault
- allow bounded recovery through reset/config path
- continue local gameplay/shell behavior without BLE where possible

## Validation Cases

1. reset/boot/config sequence reaches `BLE_IDLE` with bounded timing
2. boot timeout routes to `BLE_ERROR`
3. advertise start/stop works without leaking connection state
4. connect/disconnect events are reflected in state
5. pairing/bonding completes and persists a record safely
6. pairing/bonding failure preserves existing valid records
7. bounded TX/RX succeeds and returns to `BLE_CONNECTED`
8. transfer timeout routes to `BLE_ERROR`
9. quiesce during idle/advertising/connected/transfer reaches `BLE_SUSPENDED`
10. resume revalidates module readiness before accepting requests
11. recovery retries are bounded and escalate when exhausted
12. BLE remains off during normal non-multiplayer low-power operation

Related:

- [[Communication_Index]]
- [[Subsystem_State_Machines]]
- [[Power_and_Sleep_Policy]]
- [[Peripheral_Robustness_Contract]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Power_Rails]]
- [[NINA_B112_BLE_Bring-up_Runbook]]
