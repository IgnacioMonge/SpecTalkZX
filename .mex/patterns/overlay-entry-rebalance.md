# Overlay Entry Rebalance

Cold overlay entries can move between SPCTLK blocks when one block is full, but only after measuring the destination with the real linked object set.

## Rule

- Do not assume a whole feature cluster fits in the target overlay. Link a scratch build or full `make` first; object-level helpers and entry-local ASM can be much larger than the visible C wrapper.
- Moving a single entry between already-loaded overlay blocks can be resident-free when the caller only changes the packed `overlay_exec(ovl_id, entry_id)` literal.
- Do not call `overlay_exec()` from code that is already executing inside an overlay unless the current overlay code is no longer needed; loading the second overlay overwrites `ring_buffer`.
- Keep tightly coupled entries together when one entry directly calls another overlay-local symbol. If they cannot fit together, move only independent entries.
- Update entry counts and all comments/callers together so stale entry IDs do not survive review.

## Rejected / Reverted

- Full RTC migration was rejected: `SPCTLK3 + rtc_seed_ovl.asm` linked to `2022B` before adding `rtc_enable_ovl`, leaving only `26B`, and `rtc_enable_ovl()` calls `rtc_seed_ovl()`.
- A temporary move of `!tz numeric` from `SPCTLK5.OVL` entry 3 to `SPCTLK3.OVL` entry 2 measured well (`SPCTLK3=1770B`, `SPCTLK5=1777B`) but was reverted after hardware reported `!tz` overlay-load failures. Keep all `!tz` entries in SPCTLK5 unless a release/package step guarantees the matching `SPECTALK.OVL` is deployed with the TAP.

## Applies To

- `overlay/overlay_entry*.asm`
- `overlay/spectalk_ovl*.c`
- `src/user_cmds.c`
- `src/spectalk.c`
