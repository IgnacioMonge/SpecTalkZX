# Resident ASM Helper Reuse

When shrinking resident hand-written ASM, first look for values that are already available via shared helpers or preserved across nearby calls before open-coding the same loads, counters, or pointer rebuilds again.

## Rule
- For cdecl argument fetches in resident ASM, prefer `_ld_hl_ix4`, `_ld_hl_ix6`, and similar shared IX helpers over repeating `ld l,(ix+n)` / `ld h,(ix+n+1)` pairs.
- If a local flag only needs zero/nonzero semantics, initialize it with the byte-sized IX/IY low register form (`ld ixl,0`, `ld iyl,a`) and update it with `inc`/`or` instead of exact-value stores.
- If a derived count or pointer was already pushed and restored around a helper, reuse the restored register state in the next phase instead of recalculating the same `ceil()/offset` expression.
- If `0` means “use 255 as safety cap”, collapse the setup into the live register (`or a` / `jr nz` / `dec a`) instead of branching to a separate `ld ...,255` block.
- Re-measure with `make`; these local hand-tuned wins are only worth keeping if the full TAP confirms a real net byte saving.

## Applied In
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_st_stricmp()`, `_st_stristr()`, `_strip_irc_codes()`, `_tokenize_params()`, `_sb_append()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_u16_to_dec()`, `_u16_to_dec3()`, `u16_digit()`, `_str_to_u16()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_uart_drain_to_buffer()`, `_main_newline()`, `_draw_badge_dither()`, `_reapply_screen_attributes()`
