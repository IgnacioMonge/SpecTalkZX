# UTF8 Sanitize Fused Strip

Display-text cleanup is fused into `_utf8_to_ascii()`: ASCII IRC control bytes
and IRC color sequences are skipped in the ASCII copy path, while high-bit UTF-8
bytes still follow the Latin-1/smart-quote conversion paths.

Keep parser callers to one sanitize pass. Reintroducing a separate
`_strip_irc_codes` cdecl pass costs an extra walk over every displayed IRC text
line and pulled resident bytes in the old layout.

Because `_utf8_to_ascii()` is also used by wrapped server notices and
notifications, the fused helper now strips IRC formatting there too. That is
intentional display sanitization; protocol-only payloads such as NAMES should
skip the helper at the parser gate instead.
