# IRC Format Strip Contract

IRC parser text handling splits parameters and display text before stripping formatting codes. Keep those two contracts separate: tokenization may replace spaces with NULs, but message text after a trailing colon must remain intact.

## Rule
- `parse_irc_message()` must isolate trailing `:text with spaces` into `pkt_txt` before calling `tokenize_params(pkt_par, ...)`; otherwise `tokenize_params()` will correctly split on spaces and corrupt message text.
- `_utf8_to_ascii()` now owns display sanitization as well as UTF-8 folding. Its ASCII path treats IRC `^C` color arguments as optional `fg[,bg]`, where each color side has up to two decimal digits.
- The old standalone `_strip_irc_codes()` pass was deleted after the fused helper measured smaller and faster for parser display text. Do not restore it unless a new build proves a net win.

## Applied In
- `asm/spectalk_asm/70_input_lookup.asm` `_utf8_to_ascii()`
- `src/irc_handlers.c` `parse_irc_message()`
