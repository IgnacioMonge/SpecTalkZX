# BSS LDIR Size-One Guard

## Context
The CRT `code_crt_init` zeroes the compiler BSS by writing the first byte and then using `LDIR` for the remaining bytes.

## Rule
- Guard both `size == 0` and `size == 1`.
- `LDIR` with `BC == 0` is not a no-op on Z80; it wraps into a 65536-byte transfer.
- After the first `ld (hl),0`, decrement the count and skip `LDIR` when the remaining count is zero.
- Keep this protection even if the current BSS is much larger than one byte; it is a startup safety invariant, not a current-layout optimization.

## Applied In
- `asm/spectalk_asm/00_preamble.asm` `code_crt_init`
