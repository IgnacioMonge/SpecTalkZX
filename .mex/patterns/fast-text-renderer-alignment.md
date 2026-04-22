# Fast Text Renderer Alignment

When a 64-column row has both a bulk renderer and a per-character renderer, keep their vertical scanline layout identical. If one path inserts top padding and the other does not, redrawn text will jump by one pixel relative to incrementally typed or printed text.

## Rule
- Treat `_print_str64_char()` as the visual baseline for resident 64-column text unless you intentionally revalidate the whole UI.
- If the per-character path blanks scanline 0 and places glyph rows 0..6 at screen scanlines 1..7, the bulk row renderer must mirror that exact layout.
- Do not compare only character data output; also compare prompt/cursor overlays, because mismatched top padding is most visible when a bulk redraw sits next to prompt characters drawn through the normal per-character path.
- Re-measure with `make` after any alignment fix; visual consistency can cost bytes, but it should be a deliberate tradeoff with a recorded new baseline.

## Applied In
- `asm/spectalk_asm/30_rendering.asm` `print_line64_fast()`
- `asm/spectalk_asm/30_rendering.asm` `redraw_input_asm()`
