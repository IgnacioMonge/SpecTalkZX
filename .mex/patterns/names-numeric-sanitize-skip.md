# NAMES Numeric Sanitize Skip

`parse_irc_message()` may skip `utf8_to_ascii(pkt_txt)` for `353` and `366`
NAMES numerics. `_utf8_to_ascii()` now also strips IRC control/color codes, so
this is the whole display-text sanitize pass. NAMES payloads are IRC protocol
lists and status text, not free-form display text in the hot automatic
JOIN-count path.

Keep the skip before dispatch and scoped to the numeric command bytes. Do not
pay a broad helper call on every message just to identify rare numerics.

PING/PONG sanitize skipping is lower value. A measured C implementation added
24 resident/TAP bytes beyond the NAMES-only skip, so leave it out unless a later
parser rewrite can fold it into an existing command-id path for near-zero cost.

Keep `353/366` near the top of `CMD_TABLE`; the skip removes per-line cleanup
work, and the table order removes avoidable dispatch comparisons from the same
burst.

In `parse_irc_message()`, caching `pkt_cmd` in a local pointer measured as a
small resident win when shared between the sanitize gate and dispatcher tests.
Do not hoist a separate first-character byte before the sanitize gate unless a
fresh build proves it; that form measured larger in this layout.
