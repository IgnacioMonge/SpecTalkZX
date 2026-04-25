# Demoscene Render/Scroll Review

External demoscene-style review of the resident text/scroll paths produced useful constraints, but the actionable path should stay measurement-first.

## Rule

- Treat `_print_str64_char()` and `_print_line64_fast()` vertical alignment as a hard visual contract: scanline 0 blank, glyph rows on scanlines 1..7.
- Full-row UI redraws may use `_print_line64_fast()` when they overwrite the whole 64-column row and follow with any needed attribute patching. This is now the preferred path for the status bar and switcher rows, because `_print_line64_fast()` matches the per-character vertical contract.
- Do not revive the generic `_print_str64_char()` space-path experiment; it has already caused status/attribute corruption in hardware-facing testing.
- The accepted `_print_line64_fast()` fast path detects pairs that normalize to blank (space, NUL padding, control byte, or high byte), skips both `unpack_glyph` calls and the left-buffer `LDIR`, writes all 8 scanlines as zero, preserves NUL pointer semantics, and leaves attribute fill unchanged.
- The current follow-up also handles half-blank pairs locally: left-blank/right-normal uses `blank_glyph` as the left source, right-blank uses `blank_glyph` as the right source, and the lookahead path narrows the left-normal contract to avoid re-reading/re-validating the same byte. This is both a CPU win for ordinary word spacing and a size win in the latest measured build.
- `_print_line64_fast()` may use `IYL` for the byte-pair countdown, but must preserve `IY` once around the function. This avoids saving/restoring `BC` around every pair while leaving inner `B` loops and `LDIR` free to clobber `BC`.
- After a byte-pair write, `HL` is exactly one character cell below the byte just rendered (`H += 8`, `L` unchanged). Restore `H` with `sub 8` and advance `HL` directly instead of pushing the screen address a second time and popping it again in the common advance tail. Latest measured cost: +1 resident byte for less stack traffic on every byte pair.
- For left-normal/right-blank pairs, keep the just-unpacked left glyph pointer on the stack and use it directly if the right side normalizes to blank; only `LDIR` the left glyph into `plf_left_buf` when the right glyph must be unpacked too. Current measured result: the renderer keeps the CPU win for NUL/space padding and is `-2B` versus the immediate baseline.
- Earlier `_print_line64_fast()` empty-pair shapes grew by `+19B` and `+36B` but were byte-economics rejects before the shrink pass; keep them as rejected shapes, not as a reason to remove the final broader fast path.
- The `_scroll_main_zone` `~12,000 t-states` comment was corrected. Bitmap `LDIR` alone copies 4096 bytes, so treat scroll as a >100k T-state contended-screen operation until measured.
- The accepted scroll micro-optimization specializes final chat-row cleanup for row 19 using fixed bitmap/attribute addresses (`0x5060` and `0x5A60`) instead of calling `cli_internal()`; row 20 notification/status/input rows remain outside the touched area.
- `_scroll_main_zone()` may use `IYL` directly as the scanline offset. Do not preserve `C` around each `LDIR` solely for the offset; `LDIR` destroys `BC`, and reusing `IYL` avoids repeated `push bc` / `pop bc` pairs without changing copied regions.
- The fixed row-19 bitmap clear may also use `IYL` as the 8-scanline countdown and keep `A=0` live across each `LDIR`; reset `L` to `0x60` after each 32-byte clear and increment `H`. This removes the old `push bc`/`push hl`/`pop hl`/`pop bc` sequence from each scanline. Latest measured cost: +1 resident byte for materially less stack traffic per scroll.
- Before calling `_scroll_main_zone()` from `_main_newline()`, a bounded `uart_drain_to_buffer()` call is acceptable as a robustness tradeoff under IRC floods: it costs `+3B` and a small no-data poll, but reduces the chance that bytes sit in the UART while the long contended VRAM copy runs.
- `_cls_fast()` is the only current mainline stack-blit exception: full bitmap clear may temporarily move `SP` to `0x5800` and write `0x4000..0x57FF` with six 256-iteration `PUSH HL`/`PUSH DE` loops. Preserve the caller's interrupt-enabled state with `ld a,i`/saved flags and only `EI` if interrupts were enabled on entry. Latest measured cost: +23 resident bytes versus the previous `_cls_fast()` shape, with a much shorter full-screen clear than the 6143-byte `LDIR`.
- Keep stack blitting outside `_cls_fast()`, SMC blitters, smooth vertical scroll, full/shadow buffers, large scanline tables, and HALT-based sync out of the mainline unless a separate prototype proves a large win under IRC RX load.

## Applies To

- `asm/spectalk_asm/30_rendering.asm` `_print_line64_fast()`
- `asm/spectalk_asm/30_rendering.asm` `_print_str64_char()`
- `src/spectalk.c` `draw_status_bar_real()`, `switcher_render()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_scroll_main_zone()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_cls_fast()`
