# Overlay Save Fragment Discard

When an overlay reuses `overlay_slot` while IRC traffic is still live, remember that `overlay_slot` aliases `rx_line`: returning with a clean parser is not enough if the save interrupted a line mid-stream.

## Rule
- After the overlay has finished with `overlay_slot`, clear its local RX/ring indices as usual.
- Only force `rx_overflow = 1` when the call started with a partial line already live in `rx_line` (`rx_pos != 0`); a blanket post-overlay discard can eat the first valid reply after exit (for example the server's `JOIN` echo).
- `!save` is stricter: after SPCTLK4 returns, the resident wrapper must drain UART once and force `rx_overflow = 1` if any byte was captured. SD I/O can hide the start of a busy IRC line before the normal parser sees it, so those bytes are treated as a possible tail fragment.
- If the overlay itself cannot cheaply know the pre-call `rx_pos`, let the caller sample that state and clear any false-positive `rx_overflow` after return.
- Export `_rx_overflow` through `overlay_defs.asm` generation if the overlay needs to touch it; the header declaration alone is not sufficient for overlay linking.

## Applied In
- `overlay/spectalk_ovl4.c` `save_config_ovl()`
- `src/user_cmds.c` `cmd_save()`
- `src/user_cmds.c` `help_render_page()`
