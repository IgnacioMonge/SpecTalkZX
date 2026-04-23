# Overlay Exit RX Reset

When overlays are loaded into `ring_buffer`, the shared overlay-exit path must clear any parser/ring state before control returns to normal IRC processing.

## Rule
- Reset `rx_pos` and `rx_overflow` on overlay exit or overlay-load failure.
- Collapse the ring to empty with `rb_tail = rb_head` so the main loop never parses overlay bytes as IRC data.
- Prefer doing this in the shared ASM exit helper (`overlay_exit_full`) so both normal exits and load-failure paths inherit the cleanup.
- If the overlay was displayed while IRC processing continued in the background, sample `rx_pos`/`rx_overflow` before calling the shared helper and re-arm `rx_overflow=1` afterwards when needed; otherwise the helper cuts the head off the current line and the parser later prints the tail as `>< ...`.
- For repeated user-facing exits, prefer a tiny C wrapper around `overlay_exit_full()` instead of open-coding the same preserve-discard sequence at every call site.

## Applied In
- `asm/spectalk_asm.asm` `_overlay_exit_full`
- `src/spectalk.c` `overlay_exit_maybe_discard()`
