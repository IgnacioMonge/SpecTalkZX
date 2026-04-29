# Resident ASM Register Lifetime Shrink

## Context
Small ASM shrink wins often come from reusing an existing register instead of materializing a new 16-bit pair. This is safe only when the full register-pair lifetime is explicit. The `80_ui_runtime.asm` keyboard pass nearly violated this by using `DE` as a table offset after a later CAPS check had reused `D`.

## Pattern
- If a pointer/index uses `DE`, prove both `D` and `E`, not only the byte being changed. A later `add hl,de` observes the stale high byte.
- Prefer changing the scratch byte register instead of clearing the high byte again when the high byte is already proven stable.
- When a loop invariant is exact, replace recomputation with the constant form, for example row scan state that always reaches a known table offset before the final row.
- For bit-0 tests whose source byte is dead after the branch, `rrca` plus `ret c`/`jr c` can replace `bit 0,a` plus branch, but only after confirming no later code needs the unrotated value.
- For repeated-subtraction decimal helpers, keep the output digit in its final register and update it only after a successful subtraction. In `u8_div10`, `B='0'` plus `inc b` after `ld c,a` beats shuttling the ASCII digit through `A`, preserves `C` as the remainder on `ret c`, and saves bytes.
- For fixed-width decimal printing, the same rule applies: keep the tens byte as ASCII in `B`, subtract first, increment only after a successful subtraction, then recover the underflowed ones with `add a,10+'0'`.
- For small modulo-plus-one ranges, repeated subtract until carry can replace `cp limit / jr c / sub limit`; after the underflow, add `limit + 1` to restore the remainder and apply the final `+1`.
- For bit-7 tests whose source byte is dead after the branch, `add a,a` plus `jr c` can replace `cp 128` plus `jr nc`. Do not use it if the original byte in `A` is needed by the slow path.
- For digit range tests where the original byte in `A` is dead, `sub '0' / cp limit` replaces two-sided `cp low` and `cp high` checks. Values below `'0'` underflow high and fail the upper-bound compare.
- For table indexing that may cross a page, prefer putting the offset in `BC` and using `add hl,bc` over hand-updating `L` plus a carry branch, when `B/C` are dead after the index calculation.
- For ASCII case-change suppression, `last ^ current == 0x20` is a compact proof that only bit 5 changed. Still validate the folded byte is in `a..z` before suppressing, or punctuation pairs can be hidden incorrectly.
- For backward word scans with `B=0` and `C=left-bound count`, `CPDR` can replace a manual `dec hl / cp / dec c` loop. If a delimiter is found, increment `C` before returning because `CPDR` decrements `BC` after the comparison.
- For rightward scans over `_line_buffer`, a NUL sentinel can replace repeated `_line_len` limit checks only while every writer preserves `line_buffer[line_len] = 0` and cursor position is clamped to `line_len`.
- When selecting one of two constant mask pairs, load the common/default pair first and use flags from the original test after `LD` instructions. `LD` preserves flags on Z80, so the uncommon branch can patch only the changed registers.
- For exact boolean returns in `L` (`0` or `1`), `dec l / jr nz,fail` is a compact false check when `L` is dead afterwards. `0` becomes `$FF` and branches; `1` becomes `0` and falls through.
- For countdown globals whose decremented value is not needed in `A`, load their address into `HL`, test `ld a,(hl) / or a`, then use `dec (hl)` instead of `dec a / ld (nn),a`.

## Guardrails
- Do not use this for bit tests above bit 0 unless the rotate count is still a byte win and the mutated `A` is dead.
- Rebuild after register-lifetime rewrites; assembler success is not enough to prove behavior when stale high bytes are possible.
- Leave a short comment when a byte-pair invariant is non-obvious, such as `D = 0` before a table-section add.
- Do not borrow `HL` for a 16-bit add through `ex de,hl` when `HL` carries a loop invariant needed by the next iteration. In `_is_ignored()`, `HL` is the live `nick` pointer after context restore, so advancing `DE=list_ptr` via `ex de,hl / ld de,16 / add hl,de / ex de,hl` would leave `HL=16` and corrupt the next compare.
- Do not use a pre-increment loop (`ld b,'0'-1` / `inc b` / `jr c end` / `ret`) for `u8_div10` unless measured in context; it adds a final branch/return and gives up the byte win.
- Do not use a pre-increment decimal loop (`ld b,'0'-1` / `inc b` before `sub 10`) for two-digit formatting: it overcounts the failed subtraction, producing `:` as the tens digit for values 90..99.
- Do not write `jr m` on Z80; relative jumps only support `z`, `nz`, `c`, and `nc`. Use `jr nz` for the exact-boolean decrement shape, or pay for `jp m` only when sign is genuinely required.

## Applied In
- `asm/spectalk_asm/80_ui_runtime.asm` `u8_div10()`, `_set_nick_color()`, `_key_ss_arrow()`, `wb_left_clean()`, `wb_right()`, `_in_inkey()`
- `asm/spectalk_asm/30_rendering.asm` `_draw_cursor_underline()`
- `asm/spectalk_asm/50_main_output.asm` `mptp_put2()`, `_main_print()` scan/guard, `_main_print_time_prefix()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_uart_drain_to_buffer()`
- `asm/spectalk_asm/70_input_lookup.asm` `_read_key()`, `_utf8_to_ascii()`, `_sntp_process_response()`
