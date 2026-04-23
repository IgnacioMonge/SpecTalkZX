# Rendering Hotpath Byte Rules

When touching `asm/spectalk_asm/30_rendering.asm`, prefer local byte wins that also shorten the hot path before attempting larger structural rewrites.

## Rule
- If a lookup table is `ALIGN 16` and the index is known to be `0..15`, do not pay carry-fixup code for `base + index`; the low-byte add cannot overflow.
- If `p64_get_scr_base()` returns successfully, treat both `cache_scr_base` and `cache_atr_base` as valid for the current `_g_ps64_y`; do not immediately call a second cache validator for attrs.
- In `sdcc_iy` builds, do not wrap `___sdcc_enter_ix` with `push iy` / `pop iy` unless the routine or one of its callees really touches `IY`; dropping the save means the argument offsets shrink by 2.
- When `phys_x` and `scanline` already form a packed 16-bit offset, load them as a pair and use a single `add hl,de` / `add hl,bc` instead of open-coding low-byte add plus carry repair.
- When a screen/attr row base is already aligned to 32 bytes and the offset is proven `0..31` (`phys_x`, `col/2`, `start_byte`), do not materialize a 16-bit register pair just to add it; `add a,l / ld l,a` is shorter and safe because the low-byte add cannot carry into `H`.
- If a glyph routine already returns a source pointer in `HL`, consume that pointer directly; do not hardcode `glyph_buffer` again unless the caller really needs the fixed buffer address.
- If all call sites already clamp a string to the target width, do not re-check the column limit inside the per-character loop; stop on NUL and keep the loop backedge short.
- If a live screen pointer is still valid in `HL`, keep it there; do not round-trip it through `BC`/stack or save/restore it across a helper that already preserves `HL`.
- If a loop reaches the next stage only via `djnz`, treat `B=0` as an established postcondition and do not immediately zero it again before `add hl,bc` / `ldir`.
- If a notification/footer row is explicitly cleared before drawing and text advances left-to-right, an even column is the first nibble written in its byte; write it directly instead of read-modify-write preserving an empty partner nibble.

## Rejected Here
- The generic-space-path rewrite for `_print_str64_char()` was reverted after real testing: letting `blank_glyph` replace the dedicated space-clearing path saved bytes, but caused status-bar corruption and attribute mixing in practice.
- The follow-up `draw_big_char()` / `_print_big_str()` internal-`A` entry plus the `EXX` cache-miss save in `p64_get_scr_base()` were reverted together with that experiment to return to the last known-good rendering baseline.

## Applied In
- `asm/spectalk_asm/30_rendering.asm` `unpack_glyph()`
- `asm/spectalk_asm/30_rendering.asm` `_print_str64_char()`
- `asm/spectalk_asm/30_rendering.asm` `_screen_line_addr()`
- `asm/spectalk_asm/30_rendering.asm` `_clear_line()`, `_clear_zone()`, `_draw_indicator()`
- `asm/spectalk_asm/30_rendering.asm` `_notif_draw()`
- `asm/spectalk_asm/30_rendering.asm` `_print_line64_fast()`
- `asm/spectalk_asm/30_rendering.asm` `p64_set_attr()`
