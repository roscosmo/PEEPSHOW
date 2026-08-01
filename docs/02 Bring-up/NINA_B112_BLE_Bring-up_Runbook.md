# NINA-B112 BLE Bring-up Runbook

This runbook records the measured HW5 procedure for bringing up the `NINA-B112-04B` BLE module.

> [!important] HW6 reuse
> HW6 UART command transport and owner-routed `AT&D4`/DSR STOP entry now pass on
> unit 001. BLE, SPS, NFC, wake, flow-control stress, and current measurements
> still require HW6 evidence in [[HW6_Brought_Up_Tracker]]. Communication wake
> remains blocked unless separately measured and granted.

Related:

- [[BLE_Communication_Contract]]
- [[Brought_Up_Tracker]]
- [[Debug_Workflows]]

---

## Scope

This runbook covers:

- reset and strap behavior, including avoidance of accidental bootloader/factory-reset entry
- `LPUART1` with RTS/CTS validation
- module boot-ready evidence
- command/profile discovery
- advertising
- connection
- pairing/bonding persistence
- bounded TX/RX
- suspend/resume behavior

---

## Preconditions

- `LPUART1` TX/RX/RTS/CTS pins validated
- NINA control pins mapped in CubeMX
- `thComm` owns all UART and NINA GPIO behavior
- bonding storage path through `thStorage` defined

---

## Control Pin Ledger

Record exact behavior once measured.

| Signal | Pin | Safe Default | Active Polarity | Required At Boot | Notes |
|---|---|---|---|---|---|
| `NINA_SW1` | `PC4` | high-Z / no-pull | config-dependent, do not drive low unless intentionally used | no, unless bootloader/default-reset behavior is intended | low with `SWITCH_2` during startup may enter bootloader; low with `SWITCH_2` held 10 s may factory reset |
| `NINA_SW2` | `PC5` | high-Z / no-pull | config-dependent, do not drive low unless intentionally used | no, unless UART-default, bootloader, connection, or factory-reset behavior is intended | low during startup restores UART defaults; can also be used for BLE connection behavior |
| `NINA_NRST` | `PC6` | low / reset asserted while BLE off | active low | yes, reset release is controlled by `thComm` | release only after SW1/SW2/DTR/DSR safe states are confirmed |
| `NINA_DTR` | `PC7` | high-Z / no-pull until configured | module-status indication, configuration-dependent | no | may indicate system mode, SPS peer, bonded BLE connection, or GAP connection depending on NINA config |
| `NINA_DSR` | `PC8` | high-Z / no-pull until configured | host control input, configuration-dependent | no | may enter command mode, disconnect/toggle connectable status, enable/disable UART, or wake/sleep depending on NINA config |

---

## HW6 Inactive STOP Baseline

For the fitted `NINA-B112` running u-connectXpress `5.0.0`, the required inactive sequence is:

1. Hold host-controlled `NINA_DSR` (`PC8`) asserted low while issuing commands.
2. Require `AT`, `AT+UBTDM=1`, `AT+UBTCM=1`, `AT+UBTPM=1`, and `AT&D4` to return `OK`.
3. Deassert `NINA_DSR` high and allow at least one second for the documented NINA-B1 UART/STOP transition.
4. Deinitialize the host UART while leaving `NINA_NRST` released high.
5. Use reset assertion only as a bounded fault fallback, not as the normal suspended state.

`AT+UPWRMNG` is not part of this sequence. Revision 56 of the u-connectXpress AT manual lists that command for NINA-B2/W13/W15, not NINA-B1. The HW6 lifecycle probe preserves two historical command slots as explicitly skipped telemetry so an unsupported command cannot block `AT&D4` again.

---

## Validation Procedure

1. Validate safe defaults for NINA GPIO without attempting communication: `NINA_NRST` asserted low, `NINA_SW1`/`NINA_SW2`/`NINA_DTR`/`NINA_DSR` high-Z/no-pull unless the test intentionally uses them.
2. Validate reset assertion/release timing, ensuring `NINA_SW1` and `NINA_SW2` are not accidentally driven low at release.
3. Validate `LPUART1` TX/RX at the CubeMX baseline baud rate.
4. Validate RTS/CTS behavior.
5. Capture module boot-ready evidence.
6. Discover or confirm the module command/profile interface.
7. Start and stop advertising.
8. Establish and drop a connection.
9. Pair/bond and persist bonding data through `thStorage`.
10. Reboot and confirm bonding data is reused or invalidated according to policy.
11. Run bounded TX/RX.
12. Quiesce and resume without stale UART or connection state.

---

## Measured HW5 Notes

- 2026-05-30: preliminary `fw0` proof validated NINA reset release and `LPUART1` command response. `NINA_NRST` read low immediately after firmware drove `PC6` high, then read high after the boot wait.
- 2026-05-30: generated `9600` baud baseline transmitted without HAL error but produced no AT response. Runtime switch to `115200` produced an `AT` echo and `OK` response: `41 54 0d 0a 0d 0a 4f 4b 0d 0a`.
- 2026-05-30: after regenerating with `LPUART1.BaudRate=115200`, clean proof captured boot banner `+STARTUP`, `AT` echo plus `OK`, `ATI` echo plus `ERROR`, and final firmware reset assertion low. Treat `ATI` as unsupported in the current module/profile state until the correct u-connectXpress version/profile query command is validated.
- 2026-05-30: bounded identity discovery validated the general u-connectXpress identity commands. Measured identity is `NINA-B112-04B-00`, firmware `5.0.0-011`, build/profile `NINA-B11X-5.0.0-011-0-g1aaa693`, manufacturer `u-blox`, model family `NINA-B1`, serial `0141431523137804`, and `ATI10` value `B5BC4AA6C339F8EA`.
- 2026-05-30: Android `Serial Bluetooth Terminal` connected to the NINA SPS path and displayed firmware-sent text `peepshow nina sps hello`. Readback captured `+UUBTACLC` and `+UUDPC`, with `phone_sps_detected=1`, `data_mode_tx_status=0`, and `hello_tx_status=0`. User typed `hello nina`, but `phone_data_rx_len` and `phone_echo_tx_count` remained `0`; phone-to-board payload receive remains pending.
- 2026-05-30: u-blox documents `+UDSF` bit 1 for data-in-AT-command-mode, but NINA-B1 support starts at SW7.0.0. This board's module reports SW5.0.0, so inbound phone payload proof should use transparent data mode and a phone app/characteristic path that writes to the SPS RX/write characteristic.
- Continued NINA bring-up should keep `LPUART1.BaudRate=115200` unless later evidence proves another persistent module configuration.

---

## Evidence

Every successful validation must link evidence from [[Brought_Up_Tracker]].

- 2026-07-31: `EV-HW6-20260731-P5-OWNERS-004` proves bounded reset/boot, five
  required NINA-B1 AT responses, `AT&D4`, host DSR deassertion, the documented
  STOP settling interval, UART deinitialization, and no reset fallback.
- 2026-08-01: both lifecycle-v3 cycles passed host-DTR assertion, preserved
  115200-baud hardware-flow-control UART reinitialization, STOP exit, fresh
  `AT`/`OK`, repeated `AT&D4` STOP, and UART deinit without touching module
  reset. The overall run failed only on the unrelated IMU wake path; exact
  results are preserved in `EV-HW6-20260801-P5-OWNERS-005`.

Do not mark wake, flow-control stress, pairing, SPS data, NFC, current, or USB
installer interaction known-good without measured target-specific evidence.
