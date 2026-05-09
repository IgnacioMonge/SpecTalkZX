# Rendering Hotpath Byte Rules

When touching `asm/spectalk_asm/30_rendering.asm`, prefer local byte wins that also shorten the hot path before attempting larger structural rewrites.

## Rule
- If a lookup table is `ALIGN 16` and the index is known to be `0..15`, do not pay carry-fixup code for `base + index`; the low-byte add cannot overflow.
- If `p64_get_scr_base()` returns successfully, treat both `cache_scr_base` and `cache_atr_base` as valid for the current `_g_ps64_y`; do not immediately call a second cache validator for attrs.
- In `sdcc_iy` builds, do not wrap `___sdcc_enter_ix` with `push iy` / `pop iy` unless the routine or one of its callees really touches `IY`; dropping the save means the argument offsets shrink by 2.
- When `phys_x` and `scanline` already form a packed 16-bit offset, load them as a pair and use a single `add hl,de` / `add hl,bc` instead of open-coding low-byte add plus carry repair.
- When a screen/attr row base is already aligned to 32 bytes and the offset is proven `0..31` (`phys_x`, `col/2`, `start_byte`), do not materialize a 16-bit register pair just to add it; `add a,l / ld l,a` is shorter and safe because the low-byte add cannot carry into `H`.
- For ZX bitmap row bases, `(y & 7) * 32` can be `and 7` plus three `rrca` instead of five `add a,a`; only use this after the mask has cleared all high bits, and save the original row in a register that is not otherwise live.
- If a glyph routine already returns a source pointer in `HL`, consume that pointer directly; do not hardcode `glyph_buffer` again unless the caller really needs the fixed buffer address.
- In `_print_str64_char()`, keep the dedicated space/control/high-byte clear path, but it may live after the normal `p64_set_attr` return block. This lets printable `33..127` fall through directly to glyph rendering while exceptional bytes branch forward to the clear path and then jump back to the shared attr writer.
- On `p64_get_scr_base()` cache misses, `_compute_screen_base()` does not touch `B`, so `B` can temporarily hold the row byte instead of `PUSH AF/POP AF` before computing the attr base. This is a cycle win with no byte delta.
- In `unpack_glyph()`, `DE` remains the compressed-source pointer throughout high-nibble expansion, so rereading `(DE)` for the low nibble can replace `ld c,a / ld a,c`. It saves one byte and one T-state per packed source byte.
- In `_print_line64_fast()`, when the left glyph has just been unpacked, save its returned `HL` pointer above the stacked screen address and only copy it to `plf_left_buf` if the right glyph is also real and will overwrite `glyph_buffer`. For right-blank/NUL/control/high-byte padding, pop that saved pointer straight into `IX` and use `blank_glyph` as the right source.
- `_redraw_input_asm()` may render the fixed prompt byte directly because it always targets row 22 byte 0: bitmap base `0x50C0`, attr base `0x5AC0`, prompt glyph in the left nibble, blank right nibble, scanline 0 blank and seven glyph rows below. This shape is HW-OK; keep it out of generic 64-column render paths.
- For input-row attrs, `col/2` is `0..31`; form `$5AC0 + col/2` with `add a,0xC0` instead of separate `SET 7,L` and `SET 6,L`. Row 23 may still add bit 5 afterwards.
- When `_redraw_input_asm()` calls `_print_line64_fast()` for row 1 first, the helper resets `_plf_start_byte` before returning; do not add a redundant zero store before row 2 unless `_print_line64_fast()` stops owning that reset.
- If a row is visually partitioned, do not use full-row `_print_line64_fast()` on the left partition. Status row cols `54..63` are owned by the clock and connection indicator, so `draw_status_bar_real()` must render cols `0..53` through `print_status_left54_fast()` or an equivalent pair-limited path; otherwise user-count refreshes briefly erase the right side and cause flicker.
- `_line_buffer` is fixed at `$5CB6`; adding `min(line_len,62)` for the second input row cannot cross page, so `add a,l / ld l,a` is valid there. Do not reuse that shortcut for cursor/word-edit offsets that can reach 127 and cross into `$5Dxx`.
- `unpack_glyph()` only needs to materialize the 7 glyph rows consumed after the caller's explicit blank top scanline. Do not restore the old eighth zero byte unless a real consumer starts reading `glyph_buffer[7]`.
- `unpack_glyph()` should not keep its old internal space fast path while hot callers already skip blank glyph unpacking. This relies on the DAT contract that the first packed glyph, ASCII space, is `00 00 00`; verify the first 13 bytes of `SPECTALK.DAT` still end `00 00 00` after any font rebuild.
- In the `unpack_glyph()` EXX LUT loop, load `H' = font_lut_hi` once after `ld de,font_lut`; the LUT low byte plus nibble index cannot carry, so each lookup only needs `add a,e / ld l,a / ld a,(hl)`.
- `glyph_buffer` is fixed at `$5BC0` and only 7 bytes are written, so `inc l` is safe inside `unpack_glyph()`. Do not move `glyph_buffer` near a page boundary without restoring `inc hl` or proving the new low-byte range.
- If all call sites already clamp a string to the target width, do not re-check the column limit inside the per-character loop; stop on NUL and keep the loop backedge short.
- If a live screen pointer is still valid in `HL`, keep it there; do not round-trip it through `BC`/stack or save/restore it across a helper that already preserves `HL`.
- If a loop reaches the next stage only via `djnz`, treat `B=0` as an established postcondition and do not immediately zero it again before `add hl,bc` / `ldir`.
- If a low-byte offset is also needed later as a count, do not replace `ld c,a / add hl,bc` blindly. `_print_line64_fast()`'s final attr fill keeps `C=start_byte` live for `31 - start_byte`, so `add a,l / ld l,a` there is not a net win without a separate count-preserving shape.
- If a notification/footer row is explicitly cleared before drawing and text advances left-to-right, an even column is the first nibble written in its byte; write it directly instead of read-modify-write preserving an empty partner nibble.
- `_notif_draw()` and `_ikkle_draw()` may share the packed Ikkle-4 glyph blitter only after each caller has already computed its own screen base. Keep the shared helper limited to `HL=screen byte`, `DE=packed glyph`, `C=64-col x`; do not merge the notification fixed row or divider row/attr address setup.
- A local manual fill of exactly 32 bytes can be tail-called or called through `_fast_fill_attr` when `A=value`, `HL=start`, `BC=32`, and the caller does not need to preserve `A` for loop state. This is valid for `_notif_clear` attributes, `_main_hline` pixel/attr rows, and `_scroll_main_zone` row-19 attrs.
- `_notif_clear()` pixel bytes can use a local stack-clear only if the function saves/restores `SP`, runs with interrupts disabled while `SP` points at screen memory, and restores `SP` before tail-calling `_fast_fill_attr()` for attributes. This preserves the footer output and slide-in animation but costs resident bytes, so keep it as a measured responsiveness tradeoff.
- `_fast_fill_attr` must return `HL` pointing just past the filled range. Its body writes the first byte manually and then uses `LDIR` for `count-1`, so `DE` is the true past-end pointer after `LDIR`; keep the final `ex de,hl` if callers reuse `HL` for a contiguous next row.
- Do not rewrite `cli_internal`'s attribute fill through `_fast_fill_attr` unless `A=line` is restored before return. `clear_zone()` depends on `A` surviving `cli_internal` so it can `inc a` for the next row; loading `A=C` for the fill breaks that loop.
- `call helper / ret` may become `jp helper` only when the helper's `ret` should return to the original caller and no cleanup remains. This is valid for `dbc_render_bot -> dbc_mask_2sc` and `_main_clear_indent6 -> p64_get_scr_base`.

## Rejected Here
- The generic-space-path rewrite for `_print_str64_char()` was reverted after real testing: letting `blank_glyph` replace the dedicated space-clearing path saved bytes, but caused status-bar corruption and attribute mixing in practice.
- The follow-up `draw_big_char()` / `_print_big_str()` internal-`A` entry plus the `EXX` cache-miss save in `p64_get_scr_base()` were reverted together with that experiment to return to the last known-good rendering baseline.
- Reusing `sw_flags_snap[]` as a temporary switcher tab-width cache looked attractive but grew the current SDCC build by about `+20B`; keep the repeated `sw_tab_width()` call unless the switcher is rewritten more deeply.
- Replacing the `notify()` `strchr(notif_buf, 1)` plus `st_strlen()` path with a fused local C scan grew the current build by nearly `+100B`; keep the library/helper shape.
- The proposed `cli_internal` `_fast_fill_attr` rewrite saves no safe bytes as written and breaks `clear_zone()` by returning with `A=attr` instead of `A=line`. A preserving version needs `push af`/`pop af` around the fill and has no byte win.

## Applied In
- `asm/spectalk_asm/30_rendering.asm` `unpack_glyph()`
- `asm/spectalk_asm/30_rendering.asm` `_print_str64_char()`
- `asm/spectalk_asm/30_rendering.asm` `_screen_line_addr()`
- `asm/spectalk_asm/30_rendering.asm` `_clear_line()`, `_clear_zone()`, `_draw_indicator()`
- `asm/spectalk_asm/30_rendering.asm` `_notif_draw()`
- `asm/spectalk_asm/30_rendering.asm` `_print_line64_fast()`
- `asm/spectalk_asm/30_rendering.asm` `p64_set_attr()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_fast_fill_attr()`
