# Keyboard Translation Table Shrink

`_in_inkey()` is already close to the useful size floor. Keep byte wins local and measured.

## Rule

- Keep the explicit 40-byte CAPS table unless a replacement proves smaller in `make`.
- Do not derive CAPS letters plus a small digit/arrow exception table blindly. The tested version grew the resident binary by 9B because the extra branch logic cost more than the removed table bytes.
- Safe micro-shrink: if `D` only exists to zero-extend the table index for `add hl,de`, initialize `DE=0` at entry and do not reload `D=0` after the bit scan.
- Safe micro-shrink: the three no-key row tests can use `inc a / jr nz, ik_found` if `ik_found` immediately starts with a shared `dec a` to restore the original active-low row byte before bit-scan. Do not use `inc a` without that restore; it corrupts the key-position scan.
- `key_ss_arrow()` is intentionally separate from `_in_inkey()` because `_in_inkey()` ignores both-shift chords. If SS+CS navigation consumes a key before the normal `read_key()` path, call `key_click()` from that handler on actual action/repeat ticks so click feedback is not lost.
- `_key_click()` is now a short direct EAR/MIC micro-pulse, not a call into `_beep_core`. It still blocks briefly, but should not be described as non-blocking. Preserve the `TA_BORDER & 7` border-colour restore shape when changing it; theme 3 depends on border colour not being clobbered.
- Do not let `CAPS SHIFT+5/6/7/8` arrow navigation, `CAPS SHIFT+SPACE` BREAK, or `CAPS SHIFT+SYMBOL SHIFT` word navigation drive the caps cursor indicator. `_draw_cursor_underline()` reads cached `_cursor_shift_held`; the main loop only promotes shift into that cache for alphabetic key events, and `_key_shift_held()` rejects SPACE/SYMBOL on row `$7FFE`.
- SS+CS+5/8/0/7/6 currently map to word-left, word-right, delete-word, input-start, input-end. Preserve this order unless the UI contract changes.
- `_input_word_right()` depends on applying the `wb_right` result through `wm_apply`. If new helpers are inserted before `wm_apply`, add an explicit `jr wm_apply`; otherwise word-right falls into the next helper body.

## Applies To

- `asm/spectalk_asm/80_ui_runtime.asm::_in_inkey`
- `asm/spectalk_asm/80_ui_runtime.asm::_key_ss_arrow`
