# Resident ASM Helper Reuse

When shrinking resident hand-written ASM, first look for values that are already available via shared helpers or preserved across nearby calls before open-coding the same loads, counters, or pointer rebuilds again.

## Rule
- For cdecl argument fetches in resident ASM, prefer `_ld_hl_ix4`, `_ld_hl_ix6`, and similar shared IX helpers over repeating `ld l,(ix+n)` / `ld h,(ix+n+1)` pairs when an IX frame is already needed. For simple leaf routines, direct `POP` extraction followed by immediate stack restoration can be smaller and avoids `___sdcc_enter_ix` entirely.
- If a local flag only needs zero/nonzero semantics, initialize it in the cheapest register bank that survives the local helper body. `IXL`/`IYL` are useful, but `AF'` can be smaller for leaf arithmetic with no calls and no nested alternate-register contract.
- If a volatile loop counter must survive calls, compare `IYL` against a main-register counter protected by `PUSH/POP` around the call chain; keep whichever full build proves smaller.
- If a local fill/copy helper documents a useful return register, chain the next contiguous operation through that register instead of reloading the address. Current example: `_fast_fill_attr` returns `HL` at the next byte, so contiguous attribute bands can reuse `HL`.
- Underflow digit tests (`sub '0' / cp 10`) are valid only when callers need carry/range status and do not need the original `A` after the helper. If failure paths reread from memory, this is a compact safe shape.
- If a derived count or pointer was already pushed and restored around a helper, reuse the restored register state in the next phase instead of recalculating the same `ceil()/offset` expression.
- If `0` means “use 255 as safety cap”, collapse the setup into the live register (`or a` / `jr nz` / `dec a`) instead of branching to a separate `ld ...,255` block.
- Re-measure with `make`; these local hand-tuned wins are only worth keeping if the full TAP confirms a real net byte saving.

## Applied In
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_st_stricmp()`, `_st_stristr()`, `_tokenize_params()`, `_sb_append()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_u16_to_dec()`, `_u16_to_dec3()`, `u16_digit()`, `_str_to_u16()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_uart_drain_to_buffer()`, `_main_newline()`, `_draw_badge_dither()`, `_reapply_screen_attributes()`
