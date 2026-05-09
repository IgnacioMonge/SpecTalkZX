# Cold Command Overlay Migration

Move cold resident commands to overlays only when the command does not sit on the IRC RX hot path and the destination overlay has measured headroom.

## Rule

- Keep the resident wrapper small: copy bounded args into `overlay_slot`, call `overlay_exec(ovl_id, entry_id)`, and preserve the existing partial-RX loss marker by setting `rx_overflow` when `rx_pos` was nonzero before the load.
- If the old command mutated the input buffer before acting, keep that behavior in the resident wrapper before copying to `overlay_slot`; otherwise overlay migration can change parsing semantics.
- Export only the minimal resident ABI needed by the overlay entry. Prefer existing helpers like `st_copy_n` over duplicating equivalent code in every overlay.
- Do not call `overlay_exec()` from an overlay entry unless the current overlay code is finished; loading another overlay overwrites `ring_buffer`.
- Measure both resident TAP and destination `SPCTLK*.OVL` size. A good move should recover resident bytes without putting the destination near 2048B.

## Applied

- `!friend` moved from resident `cmd_friend()` to `SPCTLK3.OVL` entry 2. The wrapper still truncates at the first space before copying the nick token. Build result: TAP `36031B -> 35846B`, BSS free `159B -> 344B`, SPCTLK3 `1531B -> 1860B`.

## Applies To

- `src/user_cmds.c`
- `overlay/spectalk_ovl*.c`
- `overlay/overlay_entry*.asm`
- `overlay/overlay_api.h`
- `tools/gen_overlay_defs.py`
