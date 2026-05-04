# SNTP Retry on Connect

Querying `AT+CIPSNTPTIME?` once on `/server` is not enough: the ESP8266 syncs NTP asynchronously in the background after `AT+CIPSNTPCFG`, and a fresh query may return `+CIPSNTPTIME:Thu Jan 01 00:00:00 1970` if NTP hasn't resolved yet.

## Rule
- On the `/server` connect path, query SNTP up to 3 times with ~1s delay between attempts, exiting as soon as `sntp_queried` flips to 1.
- If `sntp_tz == TZ_RTC`, `sntp_init_sent` stays clear and this path must skip SNTP entirely; RTC local time is already authoritative for the session.
- Numeric `!tz` changes clear `sntp_init_sent/sntp_waiting/sntp_queried` to force a fresh `AT+CIPSNTPCFG` with the new timezone. While already in IRC/TCP, that sync is pending; it must not send AT commands until `STATE_WIFI_OK`.
- When `/server` reconnects after a pending TZ change, call `sntp_init()` immediately after `force_disconnect()` and wait for its `OK` before `CIPSTART`, so the existing pre-transparent retry block can query NTP with the updated timezone.
- The ASM parser (`sntp_process_response`) already distinguishes the two failure modes: a `1970` response clears `sntp_waiting` only, while a valid `HH:MM:SS` sets `sntp_queried=1` and draws the status bar. Caller must check `sntp_queried` (not just line-level `OK`) to decide whether to retry.
- The parser intentionally scans forward from `+C...` until it finds `HH:MM:SS` instead of pre-counting the whole line. Keep the fixed placeholder check immediately behind the found time (`HH:MM:SS 1970`) so a cold ESP does not mark the clock as valid.
- Keep the retry inside the AT-command window (before `AT+CIPMODE=1`). Once the ESP enters transparent TCP mode, `sntp_query_time()` refuses to run and the clock cannot be recovered until disconnect.
- `BREAK` (Symbol+Space) must abort all retries, not just the current wait.

## Why
- First NTP resolution after ESP boot depends on DNS cache and server responsiveness; typical cold-cache latency is 1–4s.
- A single 2s query inside `/server` loses the race often enough to be observed as "clock sometimes stays at 00:00 after connect", with no way to recover without reconnecting.
- Main loop retries every ~1.5s while `STATE_WIFI_OK`, but autoconnect fires at ~5s so the first real chance to sync is often already inside the connect sequence.

## Applied In
- `src/user_cmds.c` `/server` path, between `CIPSTART` success and `CIPMODE=1`.
