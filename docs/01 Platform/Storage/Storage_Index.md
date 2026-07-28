# Storage Index

This section defines AT25SL128A storage ownership, installer isolation, package region layout, USB MSC staging/export behavior, and recovery rules.

## Core Notes

- [[Storage_and_Installer_Contract]]
- [[Platform_Firmware_Update_and_Development_Security_Policy]]
- [[USB_Development_Mode_Contract]]
- [[USB_MSC_Bring-up_and_Recovery_Runbook]]
- [[Package_Manager_State_Machine]]
- [[Package_Contract]]
- [[Authority_and_Invariants]]

## Core Rule

The storage owner is the only firmware component that may touch external flash, FileX/LevelX, mount state, or host-exported staging/export volumes.

The Engine and Reference Game consume storage through approved package and save APIs only.
