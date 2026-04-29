# Resident ASM Preserve Tail Merge

When shrinking resident hand-written ASM, audit `IX/IY` preservation against the real local/callee contract first, then tail-merge repeated tiny endings before attempting wider structural rewrites.

## Rule
- If a helper does not touch `IY`/`IX` and every callee in that path also leaves that register alone, drop the outer `push`/`pop` instead of preserving it "just in case"; under the current `sdcc_iy` build this is a safe 4-byte win per routine.
- If several local branches end in the same tiny sequence such as `ld (var),a / ld l,b / ret`, route them through one shared tail label instead of repeating the whole epilogue.
- For restored-stack cdecl helpers, early `RET cc` exits are valid only after the original caller-cleaned argument stack has been put back exactly. Do not turn a branch into `RET cc` while temporary arguments or saved pointers are still on the stack.
- Direct `RET cc` is also valid after a local cleanup block has restored all temporary segment state. In `_main_print_wrapped_ram()`, the cancel checks happen after `next_start`, `cutpoint`, and the cut byte have been popped/restored, so `RET Z` can replace a branch to a one-byte abort tail.
- If two stack-cleanup exits differ only by a small return value, keep that value in a register not touched by the shared cleanup. In `_esx_detect()`, `A=1` or `A=0` survives `pop hl / ld (nn),hl / pop iy`, so one epilogue can restore `ERR_SP`, restore `IY`, and move `A` to `L`.
- If a bounds/overflow check only `push`es a pointer so it can be immediately `pop`ped again on the non-error path, prefer an algebraic restore or a pre-biased constant (`base + limit`) to avoid the extra stack traffic.
- For `__z88dk_callee` leaf wrappers around `LDIR/LDDR`, pop stack arguments directly into the instruction's natural operands when possible. In shift helpers, load `count` straight into `BC`, keep the return address in a dead pair, and use `RET Z` before the block move.
- For sibling esxDOS I/O routines with identical `PUSH IY/PUSH IX` stack shape, share the carry-handling epilogue. A plain `JR` from the first routine preserves the carry flag produced by `RST 8`, so one `jr nc / ld bc,0 / store result / pop ix / pop iy / ret` tail can serve both.
- Prefer native indexed loads such as `LD IX,(nn)` over `LD HL,(nn) / PUSH HL / POP IX` when the assembler accepts them.
- For cdecl cleanup wrappers that save the return address in the alternate register set with `EXX`, every callee on that path must remain free of `EXX`; otherwise the wrapper's saved return address can be overwritten invisibly.
- After a shrink pass, probe nearby local `jp` backedges/tail calls for `jr` only when the source and target are in the same section, and keep the change only if `make` proves the jump is actually back in range.

## Rejected Here
- `asm/spectalk_asm/70_input_lookup.asm` `_utf8_to_ascii()` still has `u8a_store -> u8a_loop` out of `jr` range (`-$B5` from the assembler), so that backedge must stay `jp`.

## Applied In
- `asm/spectalk_asm/20_rx_ring_uart.asm` `_try_read_line_nodrain()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_reapply_screen_attributes()`, `_cls_fast()`
- `asm/spectalk_asm/50_main_output.asm` `_main_print_time_prefix()`, `main_print_wrapped_ram()`
- `asm/spectalk_asm/10_core_helpers.asm` `_st_copy_n_cleanup()`, `_st_stricmp_cleanup()`, `_st_stristr_cleanup()`
- `asm/spectalk_asm/60_protocol_storage.asm` `_esx_detect()`, `_text_shift_right()`, `_text_shift_left()`, `_main_puts2()`, `_esx_fread()`, `_esx_fwrite()`
- `asm/spectalk_asm/70_input_lookup.asm` `_read_key()`
