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
- `tokenize_params()` is a single-callsite parser helper. Keep its ABI as
  `void tokenize_params(char *par) __z88dk_fastcall`: `parse_irc_message()`
  always uses the default 10 IRC params, `split_next_param()` already removes
  leading whitespace before `pkt_par`, and trailing `:` text is isolated before
  tokenization. The tokenizer can therefore use fixed `B=10`, compute
  `_irc_param_count` once at exit as `10-B`, use `DE` as the `_irc_params`
  cursor, and skip old cdecl/max-param/IY/leading-colon/NULL-pointer guards.
- If deferring tokenization, keep it lazy and central rather than open-coding
  calls in every handler. `parse_irc_message()` should mark params dirty after
  trailing-text isolation; `irc_param()` should ensure tokenization before
  returning a param. Audit non-`irc_param()` dependencies: `JOIN` still needs
  the first param split for IRCv3 extended-join, and direct `irc_params[]`
  loops such as network `005` and generic numeric display must explicitly
  ensure before reading the array. Raw-param handlers such as `CAP` should not
  force tokenization. This lazy shape was HW OK and produced a large perceived
  parser speedup.
- For prefixed IRC lines, keep prefix split and nick isolation fused in
  `split_prefix_nick()` rather than doing `split_next_param(pkt_usr)` followed
  by a second `isolate_nick(pkt_usr)` walk. The helper must:
  - start at `pkt_usr` (`line + 1`),
  - write NUL at the first `!` if present,
  - continue scanning until the first byte `<33`,
  - write NUL at that separator and skip following separators,
  - return the command start or a pointer to NUL when no command exists.
  The compact measured form reuses `split_next_param_found` for the separator
  tail; duplicating that skip logic grew the build. Final measured build in
  `codex/irc-parser-improve`: TAP `35191B`, BSS free `1001B`, overlays
  unchanged, `-4B` versus the lazy-tokenizer baseline.
- Inline 3-digit numeric parsing was rejected after build: it grew TAP by 331 bytes.
- Replacing the hot-ordered `CMD_TABLE` linear dispatcher with a large C
  `switch(cmd_id)` was rejected after build. In the lazy-tokenizer layout, SDCC
  grew the TAP by 371 bytes instead of shrinking it. Keep the table unless a
  future hand-written dispatcher is measured independently.
- Removing `parse_irc_message()`'s `!line` guard was rejected after build
  because it grew the TAP sharply in this layout. Keep the original
  `if (!line || !*line)` guard unless a fresh compiler layout proves otherwise.
- In the lazy-tokenization shape, keeping `irc_params_ensure()` as a normal C
  helper measured smaller than inlining it into `irc_param()` or rewriting it
  as a naked ASM tail-call.

## Applies To

- `src\irc_handlers.c`
- `asm\spectalk_asm\40_text_numeric_screen.asm`
