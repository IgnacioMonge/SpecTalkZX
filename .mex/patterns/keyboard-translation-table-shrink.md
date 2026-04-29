# Keyboard Translation Table Shrink

`_in_inkey()` is already close to the useful size floor. Keep byte wins local and measured.

## Rule

- Keep the explicit 40-byte CAPS table unless a replacement proves smaller in `make`.
- Do not derive CAPS letters plus a small digit/arrow exception table blindly. The tested version grew the resident binary by 9B because the extra branch logic cost more than the removed table bytes.
- Safe micro-shrink: if `D` only exists to zero-extend the table index for `add hl,de`, initialize `DE=0` at entry and do not reload `D=0` after the bit scan.
- `key_ss_arrow()` is intentionally separate from `_in_inkey()` because `_in_inkey()` ignores both-shift chords. If SS+CS navigation consumes a key before the normal `read_key()` path, call `key_click()` from that handler on actual action/repeat ticks so click feedback is not lost.
- SS+CS+5/8/0/7/6 currently map to word-left, word-right, delete-word, input-start, input-end. Preserve this order unless the UI contract changes.
- `_input_word_right()` depends on applying the `wb_right` result through `wm_apply`. If new helpers are inserted before `wm_apply`, add an explicit `jr wm_apply`; otherwise word-right falls into the next helper body.

## Applies To

- `asm/spectalk_asm/80_ui_runtime.asm::_in_inkey`
- `asm/spectalk_asm/80_ui_runtime.asm::_key_ss_arrow`
