# Overlay Local Helper Relocation

If a resident helper is only imported by one overlay, it may be better to move the helper into that overlay and remove it from the generated overlay ABI.

## Rule
- First free enough space in the target overlay; do not trade a resident win for an overlay overflow.
- Keep the helper ABI byte-for-byte compatible for the overlay C object (`__z88dk_callee`, return register, small-int tricks, stack cleanup).
- Remove the helper from `tools/gen_overlay_defs.py` only after a full grep proves no other overlay or resident C callsite needs it.
- Prefer placing tiny ASM helpers in the overlay entry module when they are only support code for that overlay.

## Applied
- `overlay/spectalk_ovl4.c` no longer rescans `channels[]` to write `channels=`, freeing SPCTLK4 headroom.
- `_cfg_put` and `_cfg_kv` moved from `asm/spectalk_asm/80_ui_runtime.asm` to `overlay/overlay_entry4.asm`.
- `tools/gen_overlay_defs.py` no longer exports `_cfg_put/_cfg_kv`.
- Measured result in this pass after rejecting the unsafe ASM CSV parser: resident drops to `35945B` trimmed / `286B` slack while `SPCTLK4.OVL` remains under budget at `1958B`.
