# Timestamp Prefix Fast Row Render

## Context

`main_print_time_prefix()` used to print `HH:MM ` through six `_main_putc()`
calls. That path preserves every caller contract but pays repeated wrapper,
column, attribute and per-character render overhead on every timestamped IRC
line.

## Rule

A fast timestamp prefix renderer may use `_print_line64_fast()` only when
`main_col == 0`. The row renderer clears the whole 64-column row, so it is only
safe at the start of a clean append line. Keep a fallback for nonzero columns.

The fast path must:

- build `HH:MM ` in fixed scratch that `_print_line64_fast()` does not overwrite;
- call `_print_line64_fast(main_line, scratch, current_attr)` so the non-prefix
  part of the row keeps the message/background attribute;
- patch only the first three physical attribute cells to `ATTR_MSG_TIME`;
- set `main_col`, `g_ps64_col` and `wrap_indent` to `6`;
- restore/cache `g_ps64_y` and `g_ps64_attr` for the next character path.

## Current Shape

Status: rejected on 2026-04-25. The shape built cleanly but cost about +104
resident bytes and is not obviously faster for a 6-character prefix because it
renders/clears all 64 columns and fills all 32 attributes.

The implementation uses `fmt_buf` at `$5BE9` as the transient string:

```text
HH:MM \0
```

The current mainline should keep the `_main_putc()`-based timestamp print path
and only use the accepted same-minute fixed-indent clear.

## Tests

Minimum validation:

- `timestamps=on`;
- `timestamps=smart` with same-minute and minute-change messages;
- very short messages after timestamp;
- long wrapped messages after timestamp;
- scroll on row 19;
- pagination pause and BREAK cancel;
- themes where timestamp and normal message attributes visibly differ.
