# IRC Format Strip Contract

IRC parser text handling splits parameters and display text before stripping formatting codes. Keep those two contracts separate: tokenization may replace spaces with NULs, but message text after a trailing colon must remain intact.

## Rule
- `parse_irc_message()` must isolate trailing `:text with spaces` into `pkt_txt` before calling `tokenize_params(pkt_par, ...)`; otherwise `tokenize_params()` will correctly split on spaces and corrupt message text.
- `strip_irc_codes()` treats `^C` color arguments as optional `fg[,bg]`, where each color side has up to two decimal digits.
- `strip_is_digit()` returns carry set for digits and carry clear for non-digits. The `cp '9'+1` instruction already leaves carry set for `A <= '9'`; do not add `ccf`.
- For `A < '0'`, branch to an explicit carry-clear path such as `or a / ret`.

## Applied In
- `asm/spectalk_asm/40_text_numeric_screen.asm` `strip_is_digit()`
- `src/irc_handlers.c` `parse_irc_message()`
