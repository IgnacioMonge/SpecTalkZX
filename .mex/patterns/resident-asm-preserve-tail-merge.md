# Resident ASM Preserve Tail Merge

When shrinking resident hand-written ASM, audit `IX/IY` preservation against the real local/callee contract first, then tail-merge repeated tiny endings before attempting wider structural rewrites.

## Rule
- If a helper does not touch `IY`/`IX` and every callee in that path also leaves that register alone, drop the outer `push`/`pop` instead of preserving it "just in case"; under the current `sdcc_iy` build this is a safe 4-byte win per routine.
- If several local branches end in the same tiny sequence such as `ld (var),a / ld l,b / ret`, route them through one shared tail label instead of repeating the whole epilogue.
- If a bounds/overflow check only `push`es a pointer so it can be immediately `pop`ped again on the non-error path, prefer an algebraic restore or a pre-biased constant (`base + limit`) to avoid the extra stack traffic.
- After a shrink pass, probe nearby local `jp` backedges/tail calls for `jr` only when the source and target are in the same section, and keep the change only if `make` proves the jump is actually back in range.

## Rejected Here
- `asm/spectalk_asm/70_input_lookup.asm` `_utf8_to_ascii()` still has `u8a_store -> u8a_loop` out of `jr` range (`-$B5` from the assembler), so that backedge must stay `jp`.

## Applied In
- `asm/spectalk_asm/20_rx_ring_uart.asm` `_try_read_line_nodrain()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_reapply_screen_attributes()`, `_cls_fast()`
- `asm/spectalk_asm/50_main_output.asm` `_main_print_time_prefix()`, `main_print_wrapped_ram()`
- `asm/spectalk_asm/70_input_lookup.asm` `_read_key()`
