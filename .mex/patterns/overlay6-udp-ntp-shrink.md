---
name: overlay6-udp-ntp-shrink
description: Measured local shrink rules for `overlay/overlay_entry6.asm` raw UDP/NTP fallback.
---

# Overlay6 UDP/NTP Shrink

- `_frame_wait` currently preserves `BC`; `read_byte_timeout` can call it
  without saving the timeout counters. Re-check if `_frame_wait` changes.
- `_reset_rx_state` is the preferred tail target for final overlay RX cleanup
  when the local code would otherwise clear `rb_head`, `rb_tail`, `rx_pos`, and
  `rx_overflow` inline.
- DNS failure must not send `AT+CIPCLOSE`; open/send/IPD/time failures and the
  normal close path can share one close-send block before falling into
  `udp_fail`.
- `read_byte_timeout` returns CF=1 on byte-read success and CF=0 on timeout.
  Polling helpers can use `ccf / ret c` immediately after the call, then let
  `cp` define success/mismatch flags. Preserve the contract: timeout CF=1,
  mismatch CF=0/Z=0, full match CF=0/Z=1.
- `sub4` can preserve `DE/HL` to remove repeated `ntp_secs+3` reloads after a
  preceding `cmp4`; `cmp4` already preserves those registers. Do not blindly
  apply the same to `add4`: in the current file it has only one static callsite,
  so preserving pointers there does not reduce overlay size.
