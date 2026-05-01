# Cold Time Source Naked ASM Shrink

Tiny time-source command wrappers are good candidates for naked ASM when SDCC grows local buffers or generic string-copy paths.

## Rule
- Keep protocol-visible UART strings byte-for-byte identical. For SNTP, `sntp_init()` must still send `AT+CIPSNTPCFG=1,<tz>,"pool.ntp.org"` with `tz` rendered as `-12..12` without leading zeroes, and `sntp_query_time()` must still send `AT+CIPSNTPTIME?`.
- Preserve mode gates before sending AT commands: skip SNTP when `sntp_tz == TZ_RTC`, when SNTP init was already sent, or when `connection_state != STATE_WIFI_OK`.
- For `!tz`, preserve the exact split: lowercase `rtc` dispatches to `SPCTLK5.OVL` entry 2; all other inputs are copied into `overlay_slot` with the same bounded 8-byte copy contract and dispatched to `SPCTLK5.OVL` entry 3. Keep the resident wrapper in C unless a new naked ASM version passes hardware; the first naked wrapper measured smaller but produced `Overlay load failed` on hardware.
- Numeric TZ handling in entry 3 must stay compact: SPCTLK5 is currently near the 2048B ceiling, so prefer ASM and short feedback (`sync`/`later`) over C helpers.
- Keep `rx_overflow` behavior identical when an overlay call discards a live partial line: snapshot `rx_pos` before `overlay_exec()` and set `rx_overflow=1` only if it was non-zero.
- Use naked ASM here only for cold wrappers with small, explicit ABI surfaces and a hardware check. Do not move `/server` or UART-waiting connect loops into overlays.

## Applied In
- `src/user_cmds.c` `cmd_tz()` is intentionally C after the hardware regression.
- `overlay/overlay_entry5.asm` `_tz_cmd_ovl`
- `src/spectalk.c` `sntp_init()`
- `src/spectalk.c` `sntp_query_time()`
