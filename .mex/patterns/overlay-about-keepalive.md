# Overlay About Keepalive

`OVERLAY_ABOUT` reuses `ring_buffer` for overlay code, so normal IRC parsing cannot run there. Keepalive still has to work while the overlay is open.

## Rule
- Do not pause the main keepalive sender just because `overlay_mode == OVERLAY_ABOUT`; sending `PING :keepalive` does not depend on `ring_buffer`.
- Extend the lightweight `overlay_keepalive()` scanner so it consumes `PONG` as well as replying to server `PING`.
- If a keepalive timeout fires during `ABOUT`, close the overlay first and only then print the timeout/disconnect UI, so error text does not render over the overlay.
- Accept that non-keepalive IRC traffic is still discarded during `ABOUT`; this pattern is for avoiding false disconnects, not for full background IRC processing.

## Applied In
- `src/spectalk.c` main keepalive block
- `src/spectalk.c` `overlay_keepalive()`
