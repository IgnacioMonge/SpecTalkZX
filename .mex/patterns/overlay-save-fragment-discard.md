# Overlay Save Fragment Discard

When an overlay reuses `overlay_slot` while IRC traffic is still live, remember that `overlay_slot` aliases `rx_line`: returning with a clean parser is not enough if the save interrupted a line mid-stream.

## Rule
- After the overlay has finished with `overlay_slot`, clear its local RX/ring indices as usual.
- Then force `rx_overflow = 1` before returning so the next `\n` discards the truncated tail instead of parsing it as a fresh IRC command.
- Export `_rx_overflow` through `overlay_defs.asm` generation if the overlay needs to touch it; the header declaration alone is not sufficient for overlay linking.

## Applied In
- `overlay/spectalk_ovl4.c` `save_config_ovl()`
