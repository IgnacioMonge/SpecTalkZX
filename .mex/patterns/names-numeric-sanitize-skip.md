# NAMES Numeric Sanitize Skip

`parse_irc_message()` may skip `strip_irc_codes(pkt_txt)` and
`utf8_to_ascii(pkt_txt)` for `353` and `366` NAMES numerics. Those payloads are
IRC protocol lists and status text, not free-form display text in the hot
automatic JOIN-count path.

Keep the skip before dispatch and scoped to the numeric command bytes. Do not
pay a broad helper call on every message just to identify rare numerics.

PING/PONG sanitize skipping is lower value. A measured C implementation added
24 resident/TAP bytes beyond the NAMES-only skip, so leave it out unless a later
parser rewrite can fold it into an existing command-id path for near-zero cost.

Keep `353/366` near the top of `CMD_TABLE`; the skip removes per-line cleanup
work, and the table order removes avoidable dispatch comparisons from the same
burst.
