# USB MSC Bring-Up and Recovery Runbook

This runbook captures the practical USB MSC bring-up sequence and regression recovery path so the same failures are not rediscovered.

Use this together with:
- `19_storage_and_installer_contract.md`
- `24_debug_workflows.md`
- `22_bringup_spec_vs_tracker.md`

---

## Scope

Use this runbook for:
- first USB MSC bring-up
- regression recovery after storage/USB changes
- host enumeration/mount failures
- intermittent USB MSC fault triage

---

## What Good Looks Like

When USB MSC is healthy:
- USB device enumerates as mass storage
- host creates disk object
- host mounts a FAT volume
- host can write/read/delete a file
- firmware reports USB active with local FAT unmounted during host ownership

If any of these fail, follow decision tree in Section 9.

---

## Non-Negotiable Invariants

- Installer mode is transport-only mode.
- Host owns FAT while MSC is active.
- MCU FileX/FAT remains unmounted while host is mounted.
- No host plus MCU writer overlap on transport volume.
- No runtime rendering/audio/gameplay work in installer mode.

---

## Stabilization Checklist (Known Regression Traps)

Verify these every time USB MSC regresses:

1. descriptor string framework count handling is correct
2. USB start/connect IRQ sequencing is explicit and ordered
3. MSC transfer request max length is correctly bounded
4. MODE SENSE transfer length is clamped to valid payload and host allocation
5. caching page write-cache policy is host-compatible
6. installer mode has enough performance headroom for probe bursts
7. storage thread endpoint/interface pointers are null-guarded

If any item is missing after regeneration/refactor, expect unstable behavior.

---

## Cold-Start Bring-Up Sequence

### 1) Firmware Prep

Recommended helper sequence (adapt helper names as needed):

```gdb
source debug.gdb
ps_storage_filex_format_wait
ps_mode_verify_flashing
ps_usb_scsi_trace_reset
```

Expected:
- transport FAT formatted successfully
- installer mode active
- SCSI trace reset

### 2) Host Enumeration Checks

PowerShell examples:

```powershell
Get-PnpDevice -PresentOnly |
  Where-Object { $_.InstanceId -match "VID_0483&PID_5710|USBSTOR\\DISK" } |
  Format-Table -Auto Status,Class,FriendlyName,InstanceId

Get-Disk |
  Where-Object { $_.BusType -eq "USB" -or $_.FriendlyName -match "USBX|Mass Storage" } |
  Format-Table -Auto Number,FriendlyName,PartitionStyle,OperationalStatus,Size

Get-Volume |
  Format-Table -Auto DriveLetter,FileSystemLabel,FileSystem,OperationalStatus,Size
```

### 3) Host Write/Read/Delete Smoke

```powershell
$drv = (Get-Volume | Where-Object { $_.FileSystem -eq "FAT" -and $_.DriveLetter } | Select-Object -First 1).DriveLetter
if (-not $drv) { throw "No FAT MSC volume found." }
$path = "$drv`:\msc_write_smoke.txt"
New-Item $path -ItemType File -Force | Out-Null
"usb msc smoke" | Set-Content $path
Get-Content $path
Remove-Item $path -Force
```

### 4) Firmware Arbitration Sanity

After host probe window:

```gdb
ps_usb_status
ps_usb_scsi_trace
```

Expected:
- USB active
- local FAT unmounted during host ownership
- no MSC failure counter increase

---

## Restart-Safe Validation Bundle

Run after USB/storage refactors:

1. reconnect soak (8 cycles)
- unplug/replug
- verify device, disk, and volume appear each cycle

2. long-idle soak (10-15 min)
- keep connected in installer mode
- verify no hardfault and no escalating MSC failures

3. host smoke IO
- write/read/delete file succeeds after soak

---

## Evidence Capture Template

For each attempt capture:
- host state (`Get-PnpDevice`, `Get-Disk`, `Get-Volume`)
- firmware state (`ps_usb_status`, `ps_usb_scsi_trace`)
- packet capture file when ambiguous (`.pcapng`)
- hardfault record if crash (`ps_hf`)

Never claim root cause without trace or fault evidence.

---

## Debug Method That Preserves Evidence

Do:
- run target during host enumeration
- halt after initial host probe window
- reset trace before each reconnect attempt

Do not:
- halt repeatedly in USB IRQ hot path and treat that as runtime truth
- place breakpoints in high-frequency USB or DMA callbacks
- run mixed shell commands without proper separators

---

## Symptom to Action Decision Tree

Case A: no USB mass-storage device appears
- verify installer mode active
- verify USB start/connect path success
- verify init stage reached

Case B: USB device appears but no disk object
- inspect SCSI command progression
- inspect descriptor integrity
- inspect MODE SENSE/transfer clamp behavior

Case C: disk appears but no volume
- verify FAT formatting path
- verify media status and capacity responses

Case D: volume mounts then drops intermittently
- run long-idle and reconnect soak
- collect fault record and trace

Case E: hardfault while USB plugged
- capture `ps_hf` immediately
- inspect storage-thread pointer/endpoint safety and null guards

---

## Sign-Off Criteria

USB MSC is sign-off ready only when all pass:
1. reconnect soak passes all cycles without reflash/reset
2. host write/read/delete smoke passes
3. long-idle soak passes with no hardfault
4. arbitration invariants hold during host mount window

Record final evidence in `brought_up.md`.

