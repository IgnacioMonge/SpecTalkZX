---
name: overlay5-tz-shrink
description: Measured local shrink rules for `overlay/overlay_entry5.asm` `!tz`.
---

# Overlay5 TZ Shrink

- Overlay calls to resident helpers must be both `PUBLIC` in the resident ASM and
  whitelisted in `tools/gen_overlay_defs.py`. A symbol can appear in
  `SpecTalkZX.map` and still fail overlay linkage if the generator does not
  export it.
- `_sys_puts` is safe for `!tz` label output when the old sequence was exactly
  `call _set_attr_sys`, `ld hl,<string>`, `call _main_puts`; the helper sets SYS
  attr and tail-calls `_main_puts` with the same `HL`.
- `_reset_rx_state` is safe at `tz_done` when the intended post-overlay cleanup
  is to reset ring head, ring tail, and `rx_pos`. The extra `_rx_overflow=0`
  matches the broader resident reset contract.
- Do not blindly convert early `tz_after_sign` error branches to `JR`. In the
  current layout, `tz_range` is more than +127 bytes away (`+$B1`, `+$A8`,
  `+$A3` before later edits), so the literal `JP -> JR` proposal is invalid
  unless the error block is deliberately relocated and re-measured.
- Relocating `tz_range` upward can make those three branches relative, but it
  needs an explicit `jr tz_done` after `_ui_err` to preserve the RX-reset cleanup
  path. In the current OVL5 branch this measured as only `-1B` net.
