# Audio Index

This section defines target-gated audio ownership, sampled speaker playback, optional procedural output, DMA refill discipline, and power coordination. HW6 retains the speaker and removes the PAM/piezo BBB path.

## Core Notes

- [[Audio_Contract]]
- [[Audio_API_Contract]]
- [[Subsystem_State_Machines]]
- [[Power_and_Sleep_Policy]]
- [[HW6_Hardware_Revision_Contract]]

## Boundary

The Engine and Reference Game may request symbolic music, SFX, and only those optional audio capabilities granted by the selected target profile. HW6 blocks `audio.bbb`.

The package-facing API boundary is defined in [[Audio_API_Contract]].

On HW6, the Platform audio owner owns SAI1, GPDMA audio transfer, `SD_MODE`, mixer/decoder state, amp control, and sleep coordination. LPTIM1 remains an LPBAM timing resource and is not an HW6 audio output.
