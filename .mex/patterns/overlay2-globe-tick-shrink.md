---
name: overlay2-globe-tick-shrink
description: Measured local shrink rules for `overlay/overlay_entry2.asm` globe tick.
---

# Overlay2 Globe Tick Shrink

- `_esx_fread` leaves the actual read length in `BC` after the resident epilogue
  stores `_esx_result`, because the return path only pops `IX/IY` before `ret`.
  SPCTLK2 can compare `BC` directly when it only needs the just-read length.
- `_esx_fseek_set` returns `HL=1` on success and `HL=0` on error. In overlay
  checks that need zero on success afterward, `dec l / jr nz,error / ld a,l`
  is smaller than `ld a,l / or a / jr z,error / xor a`.
- These rules depend on the current resident helper contracts in
  `asm/spectalk_asm/60_protocol_storage.asm`; re-check if those helpers change.
