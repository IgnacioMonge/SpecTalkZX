# Keyboard Translation Table Shrink

`_in_inkey()` is already close to the useful size floor. Keep byte wins local and measured.

## Rule

- Keep the explicit 40-byte CAPS table unless a replacement proves smaller in `make`.
- Do not derive CAPS letters plus a small digit/arrow exception table blindly. The tested version grew the resident binary by 9B because the extra branch logic cost more than the removed table bytes.
- Safe micro-shrink: if `D` only exists to zero-extend the table index for `add hl,de`, initialize `DE=0` at entry and do not reload `D=0` after the bit scan.

## Applies To

- `asm/spectalk_asm/80_ui_runtime.asm::_in_inkey`
