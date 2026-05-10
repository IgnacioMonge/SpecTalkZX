---
name: earth-rtc-overlay-shrink
description: Measured local shrink rules for ABOUT Earth and RTC seed overlays.
---

# Earth / RTC Overlay Shrink

- ABOUT footer rendering is shared between normal render completion and disk
  failure fallback. Keep a single `about_draw_foot` block so fallback still
  shows the exit hint before RX reset.
- ABOUT exit must clear `_rx_last_len` in addition to the ring parser state.
  `_reset_rx_state` covers `_rb_head`, `_rb_tail`, `_rx_pos`, and
  `_rx_overflow`, but not `_rx_last_len`.
- Caching `earth_logo_attr_value` outside the logo attr row loop is a speed-only
  tradeoff in the current layout. It measured `+4B` in SPCTLK2 because the
  one-time cache setup costs more static bytes than replacing the in-loop call.
- In `rtc_seed_ovl.asm`, falling through from `rtc_esx_epilogue` to
  `rtc_try_msdos_regs` is safe after `ret c`; do not re-add the no-op `jr`.
- `bcd_to_bin` keeps low-nibble validation but can omit high-nibble validation
  because all PCF8563 callers range-check the decoded final value. Re-check this
  if `bcd_to_bin` gets reused for a non-range-checked field.
