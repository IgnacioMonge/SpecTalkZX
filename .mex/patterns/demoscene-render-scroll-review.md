# Demoscene Render/Scroll Review

External demoscene-style review of the resident text/scroll paths produced useful constraints, but the actionable path should stay measurement-first.

## Rule

- Treat `_print_str64_char()` and `_print_line64_fast()` vertical alignment as a hard visual contract: scanline 0 blank, glyph rows on scanlines 1..7.
- Do not revive the generic `_print_str64_char()` space-path experiment; it has already caused status/attribute corruption in hardware-facing testing.
- The accepted `_print_line64_fast()` fast path detects pairs that normalize to blank (space, NUL padding, control byte, or high byte), skips both `unpack_glyph` calls and the left-buffer `LDIR`, writes all 8 scanlines as zero, preserves NUL pointer semantics, and leaves attribute fill unchanged.
- The current follow-up also handles half-blank pairs locally: left-blank/right-normal uses `blank_glyph` as the left source, right-blank uses `blank_glyph` as the right source, and the lookahead path narrows the left-normal contract to avoid re-reading/re-validating the same byte. This is both a CPU win for ordinary word spacing and a size win in the latest measured build.
- `_print_line64_fast()` may use `IYL` for the byte-pair countdown, but must preserve `IY` once around the function. This avoids saving/restoring `BC` around every pair while leaving inner `B` loops and `LDIR` free to clobber `BC`.
- Earlier `_print_line64_fast()` empty-pair shapes grew by `+19B` and `+36B` but were byte-economics rejects before the shrink pass; keep them as rejected shapes, not as a reason to remove the final broader fast path.
- The `_scroll_main_zone` `~12,000 t-states` comment was corrected. Bitmap `LDIR` alone copies 4096 bytes, so treat scroll as a >100k T-state contended-screen operation until measured.
- The accepted scroll micro-optimization specializes final chat-row cleanup for row 19 using fixed bitmap/attribute addresses (`0x5060` and `0x5A60`) instead of calling `cli_internal()`; row 20 notification/status/input rows remain outside the touched area.
- `_scroll_main_zone()` may use `IYL` directly as the scanline offset. Do not preserve `C` around each `LDIR` solely for the offset; `LDIR` destroys `BC`, and reusing `IYL` avoids repeated `push bc` / `pop bc` pairs without changing copied regions.
- Keep stack blitting, SMC blitters, smooth vertical scroll, full/shadow buffers, large scanline tables, and HALT-based sync out of the mainline unless a separate prototype proves a large win under IRC RX load.

## Applies To

- `asm/spectalk_asm/30_rendering.asm` `_print_line64_fast()`
- `asm/spectalk_asm/30_rendering.asm` `_print_str64_char()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_scroll_main_zone()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_cls_fast()`
