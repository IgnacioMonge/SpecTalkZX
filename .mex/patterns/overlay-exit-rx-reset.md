# Overlay Exit RX Reset

When overlays are loaded into `ring_buffer`, the shared overlay-exit path must clear any parser/ring state before control returns to normal IRC processing.

## Rule
- Reset `rx_pos` and `rx_overflow` on overlay exit or overlay-load failure.
- Collapse the ring to empty with `rb_tail = rb_head` so the main loop never parses overlay bytes as IRC data.
- Prefer doing this in the shared ASM exit helper (`overlay_exit_full`) so both normal exits and load-failure paths inherit the cleanup.

## Applied In
- `asm/spectalk_asm.asm` `_overlay_exit_full`
