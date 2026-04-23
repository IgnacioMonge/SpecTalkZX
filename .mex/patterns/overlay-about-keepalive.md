# Overlay About Keepalive

`OVERLAY_ABOUT` reuses `ring_buffer` for overlay code, so normal IRC parsing cannot run there. Keepalive still has to work while the overlay is open.

## Rule
- Do not pause the main keepalive sender just because `overlay_mode == OVERLAY_ABOUT`; sending `PING :keepalive` does not depend on `ring_buffer`.
- Extend the lightweight `overlay_keepalive()` scanner so it consumes `PONG` as well as replying to server `PING`.
- On `ABOUT` exit, do not call `flush_all_rx_buffers()`: that throws away parser context and can break keepalive/backlog handling.
- Instead, close the overlay, re-arm `rx_overflow=1` if a partial line was live, and run a short silent `process_irc_data()` drain with `overlay_mode=1` and input paused.
- Do not preserve an arbitrary partial `rx_line` from the lightweight scanner across exit; if the line was really part of a long `353`/topic flood, restoring it leaks a chopped tail as `>< ...` once normal parsing resumes.
- Pause user input during that quiet-drain window; otherwise replies to fresh commands (for example `/join`) can be discarded together with the backlog you are intentionally suppressing.
- If a keepalive timeout fires during `ABOUT`, close the overlay first and only then print the timeout/disconnect UI, so error text does not render over the overlay.
- Accept that non-keepalive IRC traffic is still discarded during `ABOUT`; this pattern is for avoiding false disconnects, not for full background IRC processing.

## Applied In
- `src/spectalk.c` main keepalive block
- `src/spectalk.c` `overlay_keepalive()`
