# Double Height Bright Split

Double-height 64-column text spans two physical attribute rows. The top row should force BRIGHT on and the bottom row should force BRIGHT off, regardless of the theme's stored banner attribute.

## Rule
- `_draw_big_char()` owns the split because `print_big_str()` can be used after pre-clearing rows; callers must not rely on pre-cleared attrs surviving the glyph draw.
- Keep the ink/paper bits from `_g_ps64_attr`, set bit 6 for the top row, and clear bit 6 for the bottom row.
- This keeps all themes consistent and avoids duplicating row-patch logic in every banner/header caller.

## Applied In
- `asm/spectalk_asm/30_rendering.asm` `_draw_big_char`
