# Input Cursor Shift/Arrow Contract

ZX Spectrum cursor keys are typed as `CAPS SHIFT+5/6/7/8`, but the input cursor
uses its top/bottom scanline to show effective uppercase state. Do not let arrow
navigation reuse the raw CAPS SHIFT sample for cursor rendering.

## Rule

- `_draw_cursor_underline` must use cached `_cursor_shift_held`, not call
  `_key_shift_held()` directly.
- The main loop should sample raw `shift_held` once per frame for typing, but
  only promote it into cached cursor state for alphabetic key events. Do not let
  a `CAPS SHIFT`-only frame move the cursor before the second key of an arrow
  chord arrives.
- `key_shift_held()` must reject `CAPS SHIFT+SPACE` (BREAK) and
  `CAPS SHIFT+SYMBOL SHIFT` (word-navigation prefix) as clean typing shift.
- This preserves uppercase indication while typing letters, but keeps BREAK,
  history navigation, cursor-key navigation, and SS+CS word operations from
  flashing the cursor to the caps scanline.

## Applied In

- `src/spectalk.c` main input loop
- `asm/spectalk_asm/30_rendering.asm` `_draw_cursor_underline`
- `asm/spectalk_asm/00_preamble.asm` `_cursor_shift_held` extern
