# RX Ring Line Parser Contract

The resident RX path uses `_rb_head`/`_rb_tail` as ring offsets, not absolute pointers. `_try_read_line_nodrain()` consumes those offsets into `_rx_line` and returns only complete non-empty lines.

## Rule
- Keep `RING_BUFFER_SIZE == 2048`, `RING_MASK == 0x07FF`, and `RB_MASK_H == 0x07` in sync. The ASM masks only the high byte after natural low-byte overflow.
- For ring offset increments, `res 3,h` or `res 3,d` is equivalent to `and RB_MASK_H` only because the old offset is guaranteed `0..0x07FF`; `inc` can set high-byte bit 3 but cannot set bits 4..7.
- Keep `RX_LINE_SIZE == 512` and `RX_LINE_MAX == 510`; the parser writes a NUL terminator and avoids filling the final guard bytes.
- `_rx_pos` and `_rx_last_len` are 16-bit. `trln_check_valid` may store the live `HL=_rx_pos` into `_rx_last_len` after `ld a,l / or h`; that zero test does not mutate `HL`.
- The `_rx_line + RX_LINE_MAX` algebra in the non-overflow write path depends on linker support for `symbol + constant`, and on `HL` holding `rx_pos - RX_LINE_MAX` immediately before `add hl,de`.
- `_rx_overflow` is a byte flag. It can be tested through `HL=_rx_overflow` and cleared with `ld (hl),0` when `HL` is dead on both next paths.
- Do not keep stack-cleanup labels such as `trln_overflow_pop` unless a live branch reaches them after a matching `push`. A stale `pop` in this parser is both unreachable code and a future stack-corruption trap if revived casually.
- Current `_try_read_line_nodrain()` caches `_rb_tail` in `DE` and the live `_rx_line` write pointer in `BC` for one parser pass. Commit `_rb_tail` only on valid-line and empty/partial exits. Commit `_rx_pos` only on empty/partial exit by computing `BC - _rx_line`; valid lines store `_rx_last_len` from that same pointer delta and reset `_rx_pos` to zero. The valid-line path may compute that pointer delta with bytewise subtraction from `_rx_line` specifically to preserve live `DE` without stack traffic.
- Do not move UART draining into `_try_read_line_nodrain()`. It is a no-drain parser by contract; scheduling belongs at resident call sites that are known not to be executing overlays from `ring_buffer`.
- Overflow is pointer-based now: `BC >= _rx_line + RX_LINE_MAX` means discard bytes until LF, keep `_rx_overflow` set, and do not increment `BC`. On LF with overflow set, clear the flag, reset `BC` to `_rx_line`, and continue scanning for the next line.

## Rejected Here
- Rewriting `trln_return_0` to bytewise `BC - _rx_line` is currently the same size as the existing 16-bit `SBC HL,DE` sequence, so keep the clearer form unless a surrounding tail merge changes the economics.
- Rewriting the overflow clear from `HL=_rx_overflow; ld (hl),0` to direct absolute load/store grows the code; rewriting the overflow set through `HL` is equal size. Do not apply either as shrink.

## Applied In
- `asm/spectalk_asm/20_rx_ring_uart.asm` `_rb_pop()`, `_rb_push()`, `_try_read_line_nodrain()`
- `asm/spectalk_asm/10_core_helpers.asm` `_rx_pos_reset()`
