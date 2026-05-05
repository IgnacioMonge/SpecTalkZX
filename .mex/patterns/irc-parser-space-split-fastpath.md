# IRC Parser Space-Split Fastpath

## Rule

- In the hot IRC parser, replace repeated `strchr(' ')`, `skip_to(' ')`, and `skip_spaces()` combinations only when the helper preserves all token mutation semantics:
  - write NUL at the first command-token separator,
  - treat bytes `1..32` as separators on the prefix/tag/command path,
  - return the next byte greater than `32`,
  - return a pointer to NUL when there is no next token,
  - do not trim payload text where leading spaces are meaningful.
- Do not patch `h_default_cmd()` to hide numeric leaks first. Clean or
  whitespace-polluted numerics such as `002/003/004` should be normalized before
  dispatch and handled by `CMD_TABLE`/`h_ignore`.
- Keep topic and trailing-text paths separate unless the visible text contract is proven unchanged.
- Measure every arithmetic specialization. SDCC may turn simple-looking `*100 + *10` numeric code into much larger output than a call to the existing generic ASM parser.

## Current Example

- `parse_irc_message()` uses local `ST_NAKED` ASM `split_next_param()` for IRCv3 tag removal, prefix-to-command split, and command-to-params split. The helper now treats `1..32` as separators and skips following `<=32` bytes so a polluted `002` still reaches the numeric table.
- `parse_irc_message()` deliberately reuses `split_next_param()` sequentially
  at line start instead of keeping a separate normalization helper: first skip
  a leading control/space token, then a raw `>` marker token, then an IRCv3
  `@tags` token. This recovered 32B from the first tagged-message fix while
  still making tagged `PRIVMSG`/`NOTICE` dispatch normally instead of leaking
  through fallback rendering. Note the raw `>` marker tolerance is broader than
  the first helper, so if future hardware shows false positives from fragmented
  text, tighten that guard before patching `h_default_cmd()`.
- `isolate_nick()` is a separate local ASM helper for `nick!user@host` because it mutates only the nick prefix after the prefix token has already been isolated.
- Inline 3-digit numeric parsing was rejected after build: it grew TAP by 331 bytes.

## Applies To

- `src\irc_handlers.c`
- `asm\spectalk_asm\40_text_numeric_screen.asm`
