# Single-Byte Callee Pop AF

For packed trailing `uint8_t` arguments in `__z88dk_callee` helpers, `pop af` is the wrong extraction primitive on Z80 when the byte is needed as a value.

## Rule
- Do not use `pop af` to read a packed one-byte argument if the code expects the argument in `A`.
- On Z80, `pop af` loads the low stack byte into `F` and the high byte into `A`; for packed `uint8_t` args that means `A` receives the unrelated extra byte, not the argument.
- Use `pop bc`/`pop de` plus `ld a,c|e` instead, then `dec sp` to restore the caller's stack shape.
- Treat any “works because flags are dead” argument for `pop af` as suspect on packed-byte call frames.

## Applied In
- `asm/spectalk_asm/80_ui_runtime.asm` `_print_big_str`
- `asm/spectalk_asm/80_ui_runtime.asm` `_sb_put_u8_2d`
