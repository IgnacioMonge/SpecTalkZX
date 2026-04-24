# Cold Command Overlay Shrink

Cold, display-heavy local commands can move into an existing overlay when the command is not part of the live UART hot path.

## Rule

- Keep a tiny resident wrapper that records whether `rx_pos` was nonzero before `overlay_exec()`. If a partial line existed, set `rx_overflow = 1` after the overlay returns so the remaining tail is discarded instead of parsed as a fragmented IRC line.
- Overlay command bodies that print to the main log must leave `overlay_mode == 0`; otherwise `main_puts()`/`main_print()` suppress output.
- Every command overlay must reset `rb_head`, `rb_tail`, and `rx_pos` before returning, because the overlay code has overwritten `ring_buffer`.
- Do not call a resident routine from overlay code if that routine itself loads another overlay and then returns to the caller. `apply_theme()` is the concrete case: it loads SPCTLK1 for the banner, so `/theme` keeps `apply_theme()` resident and only moves the message/name display to SPCTLK1.
- Prefer SPCTLK1 for small cold command entries while it has headroom; SPCTLK2 and SPCTLK4 are already close to the 2048B block limit.

## Applies To

- `src/user_cmds.c` cold command wrappers
- `overlay/spectalk_ovl.c`
- `overlay/overlay_entry.asm`
- `overlay/overlay_api.h`
- `tools/gen_overlay_defs.py`
