# Raw UDP NTP Fallback

Some ESP AT firmwares lack `AT+CIPSNTPCFG` / `AT+CIPSNTPTIME?`, but still support DNS plus raw UDP. The working DivTIESUS `.ntpdate` path proves the viable fallback shape: resolve an NTP host, open UDP port 123, send a binary NTP request, then parse the binary `+IPD` response.

Hardware status: confirmed working on 2026-05-04, including a fresh sync after `!tz +3`.

## Rule
- Keep the fallback cold and overlay-only. Resident code should only check `sntp_init_sent && !sntp_queried` and call `SPCTLK6.OVL` entry 0.
- Use `sntp_init_sent` as a compact state byte: `1` means normal AT SNTP configured/queryable, `2` means old-AT `ERROR` observed and raw UDP should run automatically, `3` means raw UDP was attempted and the idle loop should not repeat it. Direct `/server` calls may still retry state `3` before opening TCP.
- In the idle WiFi path, trigger raw UDP only when `overlay_mode == OVERLAY_NONE`; loading SPCTLK6 while HELP/ABOUT/CONFIG is open would replace the active overlay in `ring_buffer`.
- Run it before IRC `CIPSTART`. `CIPMUX=0` allows only one active link, so the fallback cannot open UDP after the IRC TCP link exists.
- Do not call `uart_drain_to_buffer()` or `try_read_line_nodrain()` from this overlay. The overlay executes from `ring_buffer`, so RX must use direct `uartRead` polling and local overlay storage only.
- On exit, drop the overlay/RX scratch state completely: reset `rb_head`, `rb_tail`, `rx_pos`, and `rx_overflow`. Leaving `rx_overflow=1` after a raw overlay can make the next parser pass discard a valid line.
- Use the `.ntpdate`-exact transport shape: `AT+CIPDOMAIN="time.google.com"`, `AT+CIPSTART="UDP","<ip>",123`, `AT+CIPSEND=60`, send `$0B` plus 59 zero bytes, then scan for `+IPD,` and read raw bytes after the colon.
- Normal builds should fail silently. The fallback is now hardware-proven; user-visible strings such as DNS/open/send/no-reply/bad-time belong only in temporary diagnostic builds, not in the released overlay UX.
- The NTP transmit timestamp seconds are bytes 40..43 of the response, big-endian. Convert from the 2018-01-01 NTP epoch constant, apply numeric `sntp_tz` in seconds, then reduce through year/month/day/hour/minute constants until `HH:MM:SS` remains.
- Four-byte compare helpers must preserve the constant pointer if the caller will subtract the same constant next. `.ntpdate` saves the pointer around compare; in SPCTLK6 this is enforced by saving/restoring `HL/DE` inside `cmp4`.
- This fallback updates only SpecTalkZX's session clock. It does not write PCF8563 RTC; persistent RTC sync still belongs to external `.ntpdate` or a separate explicit feature.
- `SPECTALK.OVL` now has six 2048-byte blocks. Any release/deploy/test copy must keep TAP, DAT, and OVL from the same build.

## Applied In
- `src/irc_handlers.c`: old-AT `ERROR` during `CIPSNTPTIME?` response handling marks `sntp_init_sent=2`.
- `src/spectalk.c`: idle WiFi loop calls `sntp_udp_fallback()` once when state 2 is pending and no overlay is open.
- `src/user_cmds.c`: `/server` calls `sntp_udp_fallback()` before IRC `CIPSTART` if idle SNTP has not accepted a time.
- `asm/spectalk_asm/70_input_lookup.asm`: tiny resident wrapper guards `SPCTLK6` entry 0.
- `overlay/overlay_entry6.asm`: raw UDP NTP transport, binary `+IPD` read, timestamp conversion, and clock commit.
- `Makefile`: builds and packs `SPCTLK6.OVL` as block 5 of `SPECTALK.OVL`.
