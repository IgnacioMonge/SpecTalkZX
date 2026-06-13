# Server NOTICE Render Contract

Server-origin NOTICE lines such as `@time=... :server.name NOTICE nick :text`
should render in the main area with the normal message timestamp prefix, a
visible `*** ` marker, and wrapped output so long MOTD or
channel-policy notices wrap by words and keep the timestamp continuation indent.

Keep this scoped to `is_server` NOTICEs. Ordinary non-server NOTICE messages
should not be silently reformatted as server banners because that loses sender
context and broadens a cosmetic fix into unrelated IRC behavior.

`parse_irc_message()` normalizes leading line noise, strips IRCv3 tags before
command dispatch, and isolates the trailing text into writable `pkt_txt`, so
this path may pass `pkt_txt` to the `main_print_wrapped_clean()` entry. Do not
use `main_print_wrapped_ram()` for this already-sanitized `pkt_txt` path, or
server NOTICE pays a second `utf8_to_ascii()` scan before word wrapping.

The same parser contract applies to tagged `PRIVMSG`: a line like
`@time=... :nick!user@host PRIVMSG #chan :text` must reach
`h_privmsg_notice()` and use the normal channel message timestamp, nick prefix,
indentation, and word-wrap. Do not paper over these leaks in `h_default_cmd()`.

Because `parse_irc_message()` already sanitizes display text before dispatch
except for NAMES numerics and suppressed overlay output, IRC handlers should use
`main_print_wrapped_clean()` for wrapped `pkt_txt` payloads. Do not call
`main_print_wrapped_ram()` on `pkt_txt` again unless the text comes from a path
that skipped parser sanitization and will actually render.
