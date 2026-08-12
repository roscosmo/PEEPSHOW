# USBX MSC Bring-Up and Recovery Runbook

This is the active HW6 project runbook for bringing up USBX MSC installer/export mode without repeating the historical HW4/HW5 trial-and-error cycle.

This runbook preserves detailed earlier-revision failure knowledge as an HW6 verification checklist. A historical workaround or pass is not automatically an HW6 fact: verify whether the current CubeMX/USBX output still needs it, record the result in [[HW6_Brought_Up_Tracker]], then patch deliberately if required.

Use this together with:

- [[Storage_and_Installer_Contract]]
- [[USB_Development_Mode_Contract]]
- [[PMIC_and_Power_Contract]]
- [[Debug_Workflows]]
- [[Evidence_Artifact_Convention]]
- [[Bring-up_Spec_vs_Tracker]]

If this runbook conflicts with `docs/01 Platform/Authority_and_Invariants.md` or the storage/power contracts, resolve the conflict before bring-up continues.

---

## Scope

Use this runbook for:

- first USBX MSC bring-up on HW6
- regression recovery after USB/storage/CubeMX/USBX changes
- host enumeration/mount failures
- intermittent MSC hardfault triage
- deciding whether a known HW4 USBX stabilization patch is still required

This runbook validates active MSC installer/export behavior.

It does not redefine the pre-MSC gate:

- VBUS-only power attach is charging/external-power evidence only.
- USB protocol activity or successful host enumeration gates MSC availability.
- Runtime/storage quiesce and MSC export happen only after installer/export entry is accepted.

---

## 1) What Good Looks Like

### 1.1 Gate Before Active MSC

Before this runbook reaches active MSC validation:

- a charger or USB-C power bank with VBUS but no USB data host produces no MSC prompt and no storage handoff
- a real USB data host produces USB reset/enumeration or equivalent protocol activity
- PeepOS enters installer/export MSC only after the gate and entry policy allow it

### 1.2 Healthy MSC On Windows

When MSC is active and healthy on the current USBX baseline, Windows should show the device, disk, and FAT volume.

Expected baseline evidence includes:

- `Get-PnpDevice` shows the USB device node and USB storage disk node
- `Get-Disk` shows an online USB disk with the configured staging/export size
- `Get-Volume` shows a FAT staging/export volume with a drive letter
- firmware reports USB active while local FAT is unmounted during host ownership

Current known Windows strings from the prior USBX path are useful search filters:

- `USB\VID_0483&PID_5710\...`
- `USBSTOR\DISK&VEN_AZURERTO&PROD_USBX_STORAGE_DEV...`
- `AzureRTO USBX storage dev`
- `ROSCOSMO PEEPSHOW STORAGE USB Device`

If HW5 descriptors or staging size intentionally differ, record the generated descriptor/media truth in evidence before interpreting host output as a fault.

Healthy firmware state should show the equivalent of:

```text
usb: active=1
filex: mounted=0
lx_open=1
msc: fail=0
```

If the device, disk, volume, or arbitration state is missing, use the decision tree below.

### 1.3 Current HW6 Active-MSC Evidence

`EV-HW6-20260809-P7-USB-019` is the first HW6 active USBX MSC host-mount pass on `HW6-UNIT-001`.

Measured pass facts:

- Windows mounted the FAT staging volume and the user read the expected `HW6_FXLX.TXT` payload.
- The bridge handled host traffic through `thStorage`: submit/done/timeout/busy `199 / 199 / 0 / 0`.
- Media callbacks reached read/write/status `145 / 54 / 249`; dirty state became `1` after host writes.
- The exported LUN was staging-only with last LBA/block `10239 / 512`.
- USBX configured interface `0x08 / 0x06 / 0x50`, device state/config `0x3 / 1`, and no DCD IRQ guard drops.
- The successful fix was the active MSC performance floor: `SystemCoreClock=160000000`, `SysTick_LOAD=0x1869ff` during export.
- `EV-HW6-20260809-P5-CLEANUP-020` revalidates this path after extracting active export clock/PCD mechanics into `ps_hw6_usb_export`: lifecycle still passed, Windows still mounted/read `HW6_FXLX.TXT`, bridge submit/done/timeout/busy was `201 / 201 / 0 / 0`, and callbacks reached read/write/status `145 / 56 / 211`.
- `EV-HW6-20260812-P1-CLOCKUSB-037` revalidates active MSC after moving the `160 MHz` export requirement into the power-owned clock policy. Export mounted, `SystemCoreClock=160000000`, bridge submit/done/timeout/busy reached `200 / 200 / 0 / 0`, media callbacks reached read/write/status `144 / 56 / 205`, and the LUN remained `10239 / 512`.
- The same evidence validates manual reclaim: after safe eject and reclaim request, FileX/LevelX close returned `0x0`, bridge export policy was `0`, HAL PCD state was stopped, `SystemCoreClock=24000000`, and clock policy reported USB clock/VDDUSB/HSI48 all off. The USBX class deactivate callback did not increment in this controlled reclaim path; current acceptance is explicit policy-off, filesystem close success, PCD stop, and clock restore.

Do not re-open descriptor churn for a disk-without-volume symptom until the active-MSC clock/performance floor, bridge timeout/busy counters, and SCSI progression have been checked first.

---

## 2) Non-Negotiable Invariants

Always preserve:

- VBUS presence alone is not an MSC availability signal.
- USB data-host activity or enumeration must gate MSC offer/entry.
- installer/export MSC mode is USB transport mode only.
- host owns the staging/export FAT volume while MSC is active.
- MCU FileX/FAT remains unmounted while host owns the volume.
- no host plus MCU writer overlap on the FAT staging/export region.
- settings, saves, calibration, installed blobs, installed indexes, bonding records, and persistent fault logs are never host-writable.
- no active UI/audio/gameplay work in MSC installer/export mode beyond policy-limited status and exit input.
- USB MSC and USB CDC developer control are mutually exclusive personalities in v1.
- debugger-halted behavior is not runtime truth for host enumeration.

Runtime arbitration check:

```text
ps_usb_status
  filex: mounted=0
  lx_open=1
```

The status wording may change, but the ownership fact may not.

---

## 3) Required Stabilization Inventory

Do this before first serious HW5 MSC fault triage and after any CubeMX/USBX regeneration.

Do not start speculative patching until this inventory is filled in with current-HW5 status.

| ID | Historical HW4 stabilization item | Prior location | Required HW5 check | Failure signature if absent |
|---|---|---|---|---|
| MSC-STAB-001 | string descriptor framework count fix | `USBX/App/ux_device_descriptors.c` | confirm serial string append advances descriptor count correctly, historically `count += len + 1U` | malformed string descriptor or unstable early enumeration |
| MSC-STAB-002 | USB connect sequencing in device start | `Core/Src/app_threadx.c`, `AppUsbDeviceStart` | confirm start/connect/IRQ sequence is explicit: start PCD, clear pending OTG IRQ, enable IRQ, then device connect where the current USBX path requires it | intermittent descriptor transaction errors |
| MSC-STAB-003 | MSC transfer request max length bound | `USBX/App/ux_user.h` | confirm `UX_SLAVE_REQUEST_DATA_MAX_LENGTH` remains bounded for the MSC baseline, historically `512` | unstable BULK IN behavior during host probing |
| MSC-STAB-004 | MODE SENSE transfer clamp | `Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_storage_mode_sense.c` | confirm transfer length is bounded by valid response payload and host allocation, historically through `UX_MIN(...)` | MODE SENSE/MODE SELECT stalls or BOT progression wedge |
| MSC-STAB-005 | caching mode page WCE policy | `USBX/App/ux_device_class_storage.h` | confirm the caching mode-page write-cache flag policy is Windows-compatible, historically WCE forced to `0` | Windows follows MODE SELECT path that wedges BOT progression |
| MSC-STAB-006 | installer/export performance floor | power-owned clock-policy path | confirm active MSC service gets enough performance headroom for USB probe/read bursts; HW6 evidence `EV-HW6-20260809-P7-USB-019` requires `160 MHz` during active export, and `EV-HW6-20260812-P1-CLOCKUSB-037` validates that requirement through `CLK_IO_HIGH` plus reclaim back to base | timing starvation under host probe/read bursts; on HW6 this appeared as disk object present but no mounted volume until the 160 MHz export guard was added |
| MSC-STAB-007 | storage thread null guards | `Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_storage_thread.c` | inspect interface, endpoint-chain, and stall-call dereferences for null-guard coverage or confirm upstream fix | hardfault in `_ux_device_class_storage_thread`, historically precise bus fault with `BFAR=0x00000004` |

For each row record:

- `present`
- `not present and not needed on current stack`
- `missing and patched`
- `blocked / needs vendor-source review`

If a path changed in the HW5 tree, inspect the generated or middleware equivalent and record the new path in evidence.

Do not patch multiple stabilization items at once during root-cause triage. Patch one failure mechanism, rerun the same validation bundle, then continue.

---

## 4) Cold-Start Bring-Up

Follow this sequence exactly for first active MSC bring-up.

### 4.1 Before Active MSC

1. Validate VBUS-only power behavior first using the power/USB gate cases.
2. Connect to a known data-capable USB host.
3. Confirm USB data-host activity or enumeration gate passes before MSC is offered.
4. Accept or force the documented installer/export MSC entry path for the bring-up image under test.

### 4.2 Firmware Prep

For the current FW0 owner-routed debug image, use the local GDB request helper after normal lifecycle completes and the board is battery-powered with USB host VBUS attached:

```gdb
info address PS_HW6_RTOS_DebugRequestUsbExport
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_usb_msc_export_start.gdb
continue
```

After host mount, safe-eject the drive from Windows before reclaim:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_usb_msc_reclaim_start.gdb
continue
```

Do not use the detached USB parked baseline as active export prep. During active MSC export, VBUS present is expected; the detached park check exists for no-host parked state and must not reject export.

Historical HW4 helpers used `source debug.gdb`, `ps_storage_filex_format_wait`, `ps_mode_verify_flashing`, and `ps_usb_scsi_trace_reset`. Do not assume those names or numeric mode tokens remain authoritative on HW6; record the actual helper and probe values from the firmware under test.

### 4.3 Host Enumeration And Mount Checks

PowerShell:

```powershell
Get-PnpDevice -PresentOnly |
  Where-Object { $_.InstanceId -match "VID_0483&PID_5710|USBSTOR\\DISK&VEN_AZURERTO" } |
  Format-Table -Auto Status,Class,FriendlyName,InstanceId

Get-Disk |
  Where-Object { $_.BusType -eq "USB" -or $_.FriendlyName -match "AzureRTO|USBX storage dev" } |
  Format-Table -Auto Number,FriendlyName,PartitionStyle,OperationalStatus,Size

Get-Volume |
  Format-Table -Auto DriveLetter,FileSystemLabel,FileSystem,OperationalStatus,Size
```

Expected:

- USB device node present
- USB storage disk node present
- disk object online
- FAT volume mounted with a drive letter

### 4.4 Host Write/Read/Delete Smoke

PowerShell:

```powershell
$drv = (Get-Volume | Where-Object { $_.FileSystem -eq "FAT" -and $_.DriveLetter } | Select-Object -First 1).DriveLetter
if (-not $drv) { throw "No FAT MSC volume found." }
$path = "$drv`:\msc_write_smoke.txt"
New-Item $path -ItemType File -Force | Out-Null
"peepshow msc smoke" | Set-Content $path
Get-Content $path
Remove-Item $path -Force
```

### 4.5 Firmware Arbitration Sanity

After the host probe window, halt only if needed and run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_usb_state_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_clock_policy_prints.gdb
```

Historical helper aliases may print the same facts as:

```gdb
ps_usb_status
ps_usb_scsi_trace
```

Pass criteria:

- USB active during export
- local FAT unmounted during host ownership
- LevelX/media backing remains available in the intended mode
- no MSC failure counter increase
- normal SCSI progression appears in trace
- after reclaim, FileX/LevelX close succeeds, bridge export policy is off, PCD is stopped, and clock policy has restored the base profile

Historical healthy opcode flow included:

```text
0x12 INQUIRY
0x23 READ FORMAT CAPACITY
0x25 READ CAPACITY
0x28 READ(10)
```

---

## 5) Restart-Safe Validation Bundle

Run this after any USB/storage refactor and keep the output.

### 5.1 Reconnect Soak

Run eight unplug/replug cycles.

```powershell
1..8 | ForEach-Object {
  Read-Host "Cycle $_/8: unplug USB data for 2s, replug, wait 3s, then press Enter"
  Get-PnpDevice -PresentOnly |
    Where-Object { $_.InstanceId -match "VID_0483&PID_5710|USBSTOR\\DISK&VEN_AZURERTO" } |
    Format-Table -Auto Status,Class,FriendlyName,InstanceId
  Get-Disk |
    Where-Object { $_.BusType -eq "USB" -or $_.FriendlyName -match "AzureRTO|USBX storage dev" } |
    Format-Table -Auto Number,FriendlyName,PartitionStyle,OperationalStatus,Size
  Get-Volume |
    Format-Table -Auto DriveLetter,FileSystemLabel,FileSystem,OperationalStatus,Size
}
```

Pass:

- all eight cycles enumerate device, disk, and volume without reset or reflash

### 5.2 Long-Idle Fault Soak

Leave device connected in active MSC installer/export mode for at least 10-15 minutes.

Then capture:

```gdb
ps_usb_status
ps_usb_scsi_trace
```

Pass:

- no hardfault
- no escalating MSC failure count
- storage arbitration invariants still hold

### 5.3 Host Smoke IO After Soak

Repeat the write/read/delete smoke after the idle soak.

Pass:

- volume is still writable/readable by host
- firmware still owns no local FAT writer while host owns MSC

---

## 6) Debugging Method That Preserves Evidence

Do:

- run target during host enumeration/mount
- halt only after host has completed the initial probe window
- reset SCSI trace immediately before each new reconnect attempt
- save the exact host and firmware bundle for each attempt

Do not:

- halt in the USB IRQ path and treat host behavior as runtime truth
- set breakpoints in high-frequency USB/ISR paths during enumeration evidence capture
- run mixed host scripts with missing command separators
- patch multiple unrelated USB/storage failure mechanisms before rerunning the same evidence bundle

Known scripting pitfall:

```text
... | Format-Table ...  Get-Disk | ...
```

That is wrong when commands are not separated. Use newlines or semicolons between PowerShell commands.

---

## 7) Evidence Capture Bundle

For each serious MSC attempt save:

### Host State

- `Get-PnpDevice` output
- `Get-Disk` output
- `Get-Volume` output

### Firmware State

- `ps_usb_status`
- `ps_usb_scsi_trace`
- `ps_storage_status` where useful
- active mode/personality status

### Packet Capture

When host behavior is ambiguous:

- `.pcapng` filename
- one-line outcome summary, for example `mount` or `no-mount`

### Fault Record

If the target crashes:

- full `ps_hf` output
- stacked PC/LR
- CFSR/HFSR/MMFAR/BFAR
- call stack into failing function where available

Never claim root cause without a fault record, packet trace, or a repeatable state/progression trace that proves the failing point.

Record evidence using [[Evidence_Artifact_Convention]] and link it from [[Brought_Up_Tracker]].

---

## 8) Symptom To Action Decision Tree

### Case A: No USB Mass Storage Device At All

Meaning:

- device stack never reached working enumeration for the active MSC personality

Check first:

- USB data-host gate actually passed before MSC entry
- installer/export MSC mode is active
- USB start path did not fail
- current USBX init stage reached expected completion
- string descriptor/start-connect stabilization items

Historical helpers:

```text
ps_mode_verify_flashing
ps_usb_status
g_usbx_init_stage >= 101
```

Record actual HW5 equivalents.

### Case B: USB Device Appears But No USBSTOR DiskDrive

Meaning:

- transport got partway, but MSC command path did not complete to disk creation

Check:

- SCSI command progression depth/opcodes
- descriptor integrity
- MODE SENSE transfer clamp
- caching mode page/WCE policy

### Case C: USBSTOR DiskDrive Appears But `Get-Disk` Is Missing

Meaning:

- Windows has a device node, but disk object did not instantiate

Check:

- READ CAPACITY completion
- READ(10) completion
- MSC status/failure counters
- packet capture for reset/retry loops

### Case D: Disk Reports No Media Or Size Zero

Meaning:

- media status/read-capacity path failed host contract

Check:

- storage media callbacks
- media status
- read capacity response
- SCSI sense/command status trace

### Case E: Disk Online But No Volume

Meaning:

- USB transport works, but partition/filesystem is not recognized

Check:

- FAT format flow actually ran before connect
- partition style and volume enumeration
- write/read smoke once volume appears

### Case F: Mounts Then Drops Intermittently

Meaning:

- timing/race issue or crash after initial success

Check:

- long-idle MSC status counters
- hardfault evidence
- reconnect soak reproducibility
- installer/export performance floor

### Case G: HardFault While MSC Is Plugged

Immediate:

```gdb
ps_hf
```

Known historical HW4 signature:

- `_ux_device_class_storage_thread`
- `BFAR=0x00000004`
- null endpoint/interface dereference path

Check the storage-thread null-guard stabilization item before speculative changes.

---

## 9) Packet Capture Playbook

Use Wireshark USBPcap or equivalent when host-side progression is ambiguous.

### 9.1 Capture Procedure

1. Start capture before cable plug-in or MSC re-enumeration.
2. Stop after either:
   - disk and volume mount, or
   - host gives up or repeats failure cycle.
3. Save the capture in the evidence folder.

Filename example:

```text
USB_MSC_connect_try_<N>.pcapng
```

### 9.2 Useful Filters

```text
usb || usbms
usbms
usb.setup.bRequest == 6
```

### 9.3 Healthy High-Level Progression

1. descriptor requests/responses
2. set configuration
3. GET MAX LUN
4. INQUIRY
5. READ FORMAT CAPACITY / READ CAPACITY
6. TEST UNIT READY / REQUEST SENSE as needed
7. READ(10) blocks for partition/FAT probing
8. host mounts volume

### 9.4 Common Bad Signatures

| Signature | First suspicion |
|---|---|
| malformed descriptor/string response early | descriptor framework count/length issue |
| endless periodic probe retries with no disk mount | command completion or BOT phase issue |
| repeated BOT resets after one opcode | inspect matching opcode in SCSI trace and packet capture |

---

## 10) HardFault Triage Standard

At fault, run:

```gdb
ps_hf
```

Mandatory record fields:

- stacked `pc`
- stacked `lr`
- `CFSR`
- `HFSR`
- `MMFAR`
- `BFAR`
- call stack frame into the failing function where available

Do not proceed with speculative fixes without this record.

---

## 11) Fast Regression Recovery Order

If MSC regresses after unrelated changes:

1. verify the stabilization inventory
2. confirm VBUS/data-host/MSC gate sequence
3. confirm deterministic cold-start path
4. run one reconnect cycle and collect host plus firmware bundle
5. use packet capture if still failing
6. collect `ps_hf` if target crashed
7. patch one failure mechanism
8. rerun the exact same validation bundle

---

## 12) Sign-Off Criteria

MSC is sign-off ready only when all pass:

1. VBUS-only charger/power-bank attach produces no MSC prompt or storage handoff.
2. USB data-host activity/enumeration gates MSC availability and entry.
3. stabilization inventory status is recorded for the current HW6 USBX stack.
4. 8/8 reconnect soak cycles enumerate without reset/reflash.
5. host write/read/delete smoke passes.
6. 10-15 minute plugged-in idle soak passes without hardfault.
7. MSC status shows no failure escalation in the validation window.
8. arbitration invariants hold during host mount window.
9. no CDC developer interface is exposed during MSC-only validation.

Record final evidence in [[Brought_Up_Tracker]].

---

## 13) Minimal Command Block

Shortest current FW0 active-MSC validation pass:

```gdb
info address PS_HW6_RTOS_DebugRequestUsbExport
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_usb_msc_export_start.gdb
continue
```

After host mount, safe-eject the drive, reclaim, then halt after the probe window and run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_usb_msc_reclaim_start.gdb
continue
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_usb_state_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_clock_policy_prints.gdb
```

PowerShell:

```powershell
Get-PnpDevice -PresentOnly | ? { $_.InstanceId -match "VID_0483&PID_5710|USBSTOR\\DISK&VEN_AZURERTO" } | ft -Auto Status,Class,FriendlyName,InstanceId
Get-Disk | ? { $_.BusType -eq "USB" -or $_.FriendlyName -match "AzureRTO|USBX storage dev" } | ft -Auto Number,FriendlyName,PartitionStyle,OperationalStatus,Size
Get-Volume | ft -Auto DriveLetter,FileSystemLabel,FileSystem,OperationalStatus,Size
```

---

## Rule

USBX MSC bring-up must be evidence-driven.

Do not rediscover known failure classes through speculative edits when the stabilization inventory, host progression, packet capture, and fault record can identify the failing layer first.
