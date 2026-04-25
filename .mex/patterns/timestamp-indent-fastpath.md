# Timestamp Indent Fastpath

## Context

Main chat timestamps use a fixed six-column prefix (`HH:MM `). Wrapped lines and
`timestamps=smart` same-minute lines preserve that visual margin by leaving the
first six 64-column character cells blank.

## Rule

Only use fixed-width direct clear helpers for this margin while every live writer
keeps `wrap_indent` to one of two values:

- `0`: no indent;
- `6`: timestamp/wrap indent, three physical screen bytes and three attributes.

If any feature needs a different indent width, restore a generic clear path or add
a separate helper with an explicit callsite contract.

## Verified Shape

Status: build-OK and HW-OK on 2026-04-25.

`_main_clear_indent6` clears three physical bytes across the eight scanlines of
`_main_line`, writes the three attributes with `_current_attr`, sets
`main_col/g_ps64_col` to 6, mirrors `g_ps64_y/g_ps64_attr`, and prewarms the
64-column row cache for the next print.

The screen bytes are always the first three physical bytes in a 32-byte row, so
the helper may save the original low byte in `C`, use `inc l` between the three
writes, then restore `L` directly before `inc h`. The attribute writes are also
the first three cells of the row, so `inc l` is safe there too. Do not generalize
this shape to variable-width indents without restoring page/carry checks.

`_main_print_time_prefix()` must return immediately while `overlay_mode != 0`.
Most text output paths already suppress overlays, but timestamp prefix rendering
has direct screen writes through the fixed-indent helper; without this guard,
live IRC traffic consumed under `!config` can overwrite the overlay title row.

`_main_print_wrapped_ram()` needs the same overlay guard. Several IRC message
paths print nick/prefix through guarded helpers and then render the body through
`main_print_wrapped_ram()`; without the guard, live traffic can still appear in
the top-left of `!config` even when `_main_puts()` and `_main_putc()` are quiet.

The helper is shared by:

- `_main_newline()` when `wrap_indent != 0`;
- `_main_print_time_prefix()` in `timestamps=smart` mode when the minute has not
  changed.

## Tests

Minimum validation after touching this path:

- timestamps off/on/smart;
- repeated same-minute messages;
- minute change;
- long wrapped messages;
- scroll on row 19;
- pagination pause and BREAK cancel;
- colored timestamp followed by normal nick/message text.
