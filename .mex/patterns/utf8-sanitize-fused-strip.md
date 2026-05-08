# UTF8 Sanitize Fused Strip

Display-text cleanup is fused into `_utf8_to_ascii()`: ASCII IRC control bytes
and IRC color sequences are skipped in the ASCII copy path, while high-bit UTF-8
bytes still follow the Latin-1/smart-quote conversion paths.

The decoder is deliberately tolerant of mixed IRC clients. Valid `C2/C3` UTF-8
Latin-1 still wins, but malformed multi-byte sequences must not consume the
first non-continuation byte after the bad lead. Old single-byte Latin-1 accent
bytes in `C0-FF` fall back through the existing C0-FF display table, except
`C2/C3` malformed leads remain `?` so a broken `C3 20` renders as `? ` instead
of inventing `Ã`.

Keep parser callers to one sanitize pass. Reintroducing a separate
`_strip_irc_codes` cdecl pass costs an extra walk over every displayed IRC text
line and pulled resident bytes in the old layout.

Because `_utf8_to_ascii()` is also used by wrapped server notices and
notifications, the fused helper now strips IRC formatting there too. That is
intentional display sanitization; protocol-only payloads such as NAMES should
skip the helper at the parser gate instead.
