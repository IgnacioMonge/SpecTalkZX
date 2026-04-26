# Resident ASM Register Lifetime Shrink

## Context
Small ASM shrink wins often come from reusing an existing register instead of materializing a new 16-bit pair. This is safe only when the full register-pair lifetime is explicit. The `80_ui_runtime.asm` keyboard pass nearly violated this by using `DE` as a table offset after a later CAPS check had reused `D`.

## Pattern
- If a pointer/index uses `DE`, prove both `D` and `E`, not only the byte being changed. A later `add hl,de` observes the stale high byte.
- Prefer changing the scratch byte register instead of clearing the high byte again when the high byte is already proven stable.
- When a loop invariant is exact, replace recomputation with the constant form, for example row scan state that always reaches a known table offset before the final row.
- For bit-0 tests whose source byte is dead after the branch, `rrca` plus `ret c`/`jr c` can replace `bit 0,a` plus branch, but only after confirming no later code needs the unrotated value.
- For repeated-subtraction decimal helpers, keep the output digit in its final register and update it only after a successful subtraction. In `u8_div10`, `B='0'` plus `inc b` after `ld c,a` beats shuttling the ASCII digit through `A`, preserves `C` as the remainder on `ret c`, and saves bytes.
- For exact boolean returns in `L` (`0` or `1`), `dec l / jr nz,fail` is a compact false check when `L` is dead afterwards. `0` becomes `$FF` and branches; `1` becomes `0` and falls through.
- For countdown globals whose decremented value is not needed in `A`, load their address into `HL`, test `ld a,(hl) / or a`, then use `dec (hl)` instead of `dec a / ld (nn),a`.

## Guardrails
- Do not use this for bit tests above bit 0 unless the rotate count is still a byte win and the mutated `A` is dead.
- Rebuild after register-lifetime rewrites; assembler success is not enough to prove behavior when stale high bytes are possible.
- Leave a short comment when a byte-pair invariant is non-obvious, such as `D = 0` before a table-section add.
- Do not borrow `HL` for a 16-bit add through `ex de,hl` when `HL` carries a loop invariant needed by the next iteration. In `_is_ignored()`, `HL` is the live `nick` pointer after context restore, so advancing `DE=list_ptr` via `ex de,hl / ld de,16 / add hl,de / ex de,hl` would leave `HL=16` and corrupt the next compare.
- Do not use a pre-increment loop (`ld b,'0'-1` / `inc b` / `jr c end` / `ret`) for `u8_div10` unless measured in context; it adds a final branch/return and gives up the byte win.
- Do not write `jr m` on Z80; relative jumps only support `z`, `nz`, `c`, and `nc`. Use `jr nz` for the exact-boolean decrement shape, or pay for `jp m` only when sign is genuinely required.

## Applied In
- `asm/spectalk_asm/80_ui_runtime.asm` `u8_div10()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_uart_drain_to_buffer()`
- `asm/spectalk_asm/70_input_lookup.asm` `_read_key()`
