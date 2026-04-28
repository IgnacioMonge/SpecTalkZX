# Overlay Slot Bounded Config Save

## Rule

SPCTLK4 config save uses `overlay_slot`, aliased to the 512-byte `rx_line`, as its output buffer. Every append path must prove room before writing past that slot; a final `p > overlay_slot + OVERLAY_SLOT_SIZE` check is too late because adjacent BSS has already been clobbered.

## Apply

- Keep string-copy helpers bounded. If there is no room, return a sentinel greater than the slot end and let the caller report `Config too large`.
- Check any bytes written directly by C, such as `=`, `,`, `\r`, and `\n`, before storing them.
- Treat `channels=`, `friends=`, and `ignores=` as the high-risk sections because their combined size depends on runtime lists.
- Do not add resident RAM for this; the save path is cold and belongs in SPCTLK4.
- If SPCTLK4 approaches 2048B, recover space from cold status/config text before weakening bounds.
- Local SPCTLK4 ASM writers may replace C list builders when they reuse the same bounded byte primitive and return the same sentinel-on-overflow contract.

## Applied

- `overlay/overlay_entry4.asm` bounds `_cfg_put()`, `_cfg_kv()`, and `_cfg_put_autojoin()` against `overlay_slot + 512`.
- `overlay/overlay_entry4.asm` also owns `_cfg_put_friends()` and `_cfg_put_ignores()`, replacing the former C `cfg_put_csv()` loop while still routing every byte through the guarded writer.
- `overlay/spectalk_ovl4.c` composes the timezone value in a local buffer and writes `tz=` through `_cfg_kv()`; it must not raw-store `-`, `\r`, or `\n` into the slot.
