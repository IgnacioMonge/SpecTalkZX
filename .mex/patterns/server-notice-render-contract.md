# Server NOTICE Render Contract

Server-origin NOTICE lines such as `@time=... :server.name NOTICE nick :text`
should render in the main area with the normal message timestamp prefix, a
visible `*** ` marker, and `main_print_wrapped_ram(pkt_txt)` so long MOTD or
channel-policy notices wrap by words and keep the timestamp continuation indent.

Keep this scoped to `is_server` NOTICEs. Ordinary non-server NOTICE messages
should not be silently reformatted as server banners because that loses sender
context and broadens a cosmetic fix into unrelated IRC behavior.

`parse_irc_message()` strips IRCv3 tags before command dispatch and isolates the
trailing text into writable `pkt_txt`, so this path may pass `pkt_txt` to
`main_print_wrapped_ram()`.
