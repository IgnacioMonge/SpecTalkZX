# RX Ring Line Parser Contract

The resident RX path uses `_rb_head`/`_rb_tail` as ring offsets, not absolute pointers. `_try_read_line_nodrain()` consumes those offsets into `_rx_line` and returns only complete non-empty lines.

## Rule
- Keep `RING_BUFFER_SIZE == 2048`, `RING_MASK == 0x07FF`, and `RB_MASK_H == 0x07` in sync. The ASM masks only the high byte after natural low-byte overflow.
- Keep `RX_LINE_SIZE == 512` and `RX_LINE_MAX == 510`; the parser writes a NUL terminator and avoids filling the final guard bytes.
- `_rx_pos` and `_rx_last_len` are 16-bit. `trln_check_valid` may store the live `HL=_rx_pos` into `_rx_last_len` after `ld a,l / or h`; that zero test does not mutate `HL`.
- The `_rx_line + RX_LINE_MAX` algebra in the non-overflow write path depends on linker support for `symbol + constant`, and on `HL` holding `rx_pos - RX_LINE_MAX` immediately before `add hl,de`.
- `_rx_overflow` is a byte flag. It can be tested through `HL=_rx_overflow` and cleared with `ld (hl),0` when `HL` is dead on both next paths.
- Do not keep stack-cleanup labels such as `trln_overflow_pop` unless a live branch reaches them after a matching `push`. A stale `pop` in this parser is both unreachable code and a future stack-corruption trap if revived casually.

## Applied In
- `asm/spectalk_asm/20_rx_ring_uart.asm` `_rb_pop()`, `_rb_push()`, `_try_read_line_nodrain()`
- `asm/spectalk_asm/10_core_helpers.asm` `_rx_pos_reset()`
