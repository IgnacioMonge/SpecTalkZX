# Resident IRC Polish Budget

Small resident IRC UX features must piggyback on existing send/render paths, or they can blow the 48K memory guard faster than expected.

## Rule
- Prefer existing IRC send helpers such as `irc_send_cmd2()` and `irc_send_privmsg()` over one-off transport wrappers unless the new helper will be reused enough to pay for itself.
- For lightweight numeric polish such as WHOIS formatting, prefer integrating the special-case rendering into `h_numeric_default()` before adding dedicated handlers and extra `CMD_TABLE` entries.
- Rebuild after each incremental polish step. Tiny command additions plus “just a few” numeric handlers can consume the entire resident slack.
- When resident slack is down to single-digit bytes, keep the highest-value formatting only. It is better to ship `/reply`, `/notice`, and compact `311`/`319` WHOIS output than a fuller implementation that overflows `ring_buffer`.

## Applied In
- `src/user_cmds.c` `cmd_reply()`, `cmd_notice()`
- `src/irc_handlers.c` `h_numeric_default()`
- `src/spectalk.c` `force_disconnect()`
