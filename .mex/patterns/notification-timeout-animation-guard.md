# Notification Timeout Animation Guard

The ikkle notification slide-in state is split between timeout state and static animation counters. Overlay exits and disconnect paths may clear `notif_timeout` and the physical row without resetting every private animation byte.

## Rule

- The per-frame slide-in draw must require `notif_timeout != 0`, not only `notif_slide_pos < notif_slide_len`.
- Overlay mode may suppress notification drawing while IRC traffic is still parsed. If a handler calls `notify()` during that time, exiting the overlay must not let a canceled/stale timeout animate whatever bytes currently sit in `notif_buf`.
- Keep overlay footer text static during overlays: `!config`, `!about`, `!help`, and status overlays should not be overwritten by the notification animator.
- Increasing the slide step is a UX/scheduler tradeoff, not a correctness fix. `+6` chars/frame is byte-neutral against the `+3` path and halves animated redraw frames; hardware reported the stack-clear + `+6` build OK and subjectively smoother. It still does not make the slide elapsed-time based.

## Applied In

- `src/spectalk.c` main loop notification slide-in block
