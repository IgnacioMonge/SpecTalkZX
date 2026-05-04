# SNTP Retry on Connect

Querying `AT+CIPSNTPTIME?` once is not enough: the ESP8266 syncs NTP asynchronously in the background after `AT+CIPSNTPCFG`, and a fresh query may return `+CIPSNTPTIME:Thu Jan 01 00:00:00 1970` if NTP hasn't resolved yet. Older AT firmware may not support `CIPSNTPTIME` at all.

## Rule
- The normal `CIPSNTPCFG/CIPSNTPTIME` path stays in the main loop while `connection_state == STATE_WIFI_OK`; it may retry there while no IRC TCP link is open. If that query returns `ERROR`, mark raw UDP fallback pending instead of sending `CIPSNTPTIME?` forever.
- On the `/server` connect path, do not spend resident bytes on another `CIPSNTPTIME` retry loop. If the idle path has not set `sntp_queried` yet, call the raw UDP NTP fallback before IRC `CIPSTART`.
- If the idle raw-UDP attempt already ran and left `sntp_init_sent=3`, `/server` may call the fallback wrapper again. The automatic idle loop remains one-shot because it only dispatches state `2`.
- If `sntp_tz == TZ_RTC`, `sntp_init_sent` stays clear and this path must skip SNTP entirely; RTC local time is already authoritative for the session.
- If configured RTC fails at startup, RTC mode falls back to the persisted numeric backup `sntp_tz_last`, so the normal SNTP/UDP path can still run before IRC connect.
- Numeric `!tz` changes clear `sntp_init_sent/sntp_waiting/sntp_queried` to force a fresh `AT+CIPSNTPCFG` with the new timezone. While already in IRC/TCP, that sync is pending; it must not send AT commands until `STATE_WIFI_OK`.
- When `/server` reconnects after a pending TZ change, call `sntp_init()` immediately after `force_disconnect()` and wait briefly for its `OK` before `CIPSTART`, so either idle SNTP or the raw UDP fallback uses the updated timezone.
- The ASM parser (`sntp_process_response`) already distinguishes the two failure modes: a `1970` response clears `sntp_waiting` only, while a valid `HH:MM:SS` sets `sntp_queried=1` and draws the status bar. Caller must check `sntp_queried` (not just line-level `OK`) to decide whether to retry.
- The parser intentionally scans forward from `+C...` until it finds `HH:MM:SS` instead of pre-counting the whole line. Keep the fixed placeholder check immediately behind the found time (`HH:MM:SS 1970`) so a cold ESP does not mark the clock as valid.
- Keep all AT time sync inside the pre-TCP AT-command window. Once the ESP enters transparent TCP mode, the clock cannot be recovered until disconnect.
- The raw UDP fallback owns old-AT compatibility; see `raw-udp-ntp-fallback.md`.
- Normal old-AT failure should not print technical NTP diagnostics. Keep those strings for short-lived debug builds only.

## Why
- First NTP resolution after ESP boot depends on DNS cache and server responsiveness; typical cold-cache latency is 1–4s.
- A single 2s query loses the race often enough to be observed as "clock sometimes stays at 00:00 after connect", with no way to recover without reconnecting.
- Main loop retries every ~1.5s while `STATE_WIFI_OK`, but autoconnect fires at ~5s so the first real chance to sync is often already inside the connect sequence.
- The raw UDP fallback avoids both the async `CIPSNTPTIME` race and the old-firmware missing-command case, at the cost of one cold overlay load and a short UDP exchange before IRC TCP opens.

## Applied In
- `src/spectalk.c` main-loop SNTP path for `CIPSNTPTIME`.
- `src/user_cmds.c` `/server` path, before IRC `CIPSTART`, via `sntp_udp_fallback()`.
- `overlay/overlay_entry6.asm` raw UDP NTP fallback.
