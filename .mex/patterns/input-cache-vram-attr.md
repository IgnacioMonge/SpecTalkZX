# Input Cache VRAM Attr

Input redraw caching should avoid maintaining duplicate state that is never read.

## Rule

- Keep `input_cache_char` as the fast skip test for repeated input characters.
- Validate the physical attribute cell by reading VRAM directly (`0x5AC0`/`0x5AE0` rows) before skipping a redraw. Cursor underline, clears, overlays and direct redraws can change VRAM outside the character cache.
- Do not reintroduce a separate `input_cache_attr` unless a reader actually uses it. The removed version was write-only: invalidation filled it, `_put_char64_input_cached()` wrote it, and `refresh_cursor_char()` wrote it after direct redraw, but no path read it.

## Applies To

- `asm/spectalk_asm/20_rx_ring_uart.asm::_input_cache_invalidate`
- `asm/spectalk_asm/30_rendering.asm::_put_char64_input_cached`
- `src/spectalk.c::refresh_cursor_char`
