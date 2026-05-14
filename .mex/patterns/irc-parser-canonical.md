# IRC Parser Canonical Contract

`parse_irc_message()` is the single protocol parser for classic IRC and IRCv3-tagged lines. Do not add parallel IRCv3 parsing in handlers.

## Rule

- Normalize only the line head: elastically skip control/space bytes, high-bit garbage, and any leading transport marker bytes (`>`, `>>`, or spaced forms such as `> >`) before looking for IRCv3 tags, prefix, or command.
- Consume at most one IRCv3 `@tags` block before the prefix/command by scanning to the next protocol separator (`<=32`). Ignore tag contents completely; do not parse `time=` in the hot path, and do not treat high-bit bytes inside tags as separators. After the tag separator, skip control/space and high-bit residue before the prefix/command. If the next token also starts with `@`, drop the line as malformed.
- Consume an optional IRC prefix (`:nick!user@host` or `:server`) and isolate `pkt_usr` at the first `!` when present.
- Header/command token scanning treats high-bit bytes (`>=127`) as command terminators, not command content. Its skip phase must not consume high-bit bytes after an ASCII separator, because the first parameter may legitimately start with UTF-8/Latin-1. Normal middle-param splitting preserves high-bit bytes so channel names and parameters are not cut. Prefix nick isolation scans the whole `:nick!user@host` prefix until a real separator (`<=32`), preserving high-bit bytes in nick and hostmask; only the line-head, post-tags, and post-prefix skips may ignore high-bit residue before the command.
- Split the command once. Text-command dispatch requires the first two command bytes to be ASCII letters before uppercasing; malformed punctuation/control command tokens are dropped silently before fallback. Dispatch only after those first two command bytes are normalized to uppercase; keep the existing lowercase-command fallback filter for fragments and post-cancel garbage.
- Numeric dispatch is accepted only for exact three-digit command tokens (`DDD`). Fragments that start inside IRCv3 `@time=` values such as `2026-...`, `08:21...`, or `418Z` must not enter `h_numeric_default()`.
- Unknown-command fallback is visible only for lines that had a real IRC prefix and an ASCII-letter command token. Unknown no-prefix tokens such as `RIVMSG`, `.418Z`, or other mid-line tails are fragments and must be silent.
- Invalid numeric-looking command tokens are always silent fragments. Do not delegate them to `h_default_cmd()`: a mid-line `:` inside URLs or ports can fabricate a false prefix and otherwise render tails such as `12 ... PRIVMSG`.
- Internal contract after parse:
  - `pkt_par` is the first middle parameter, or `pkt_empty`.
  - `pkt_rest` is the remaining middle-parameter tail, or `pkt_empty`.
  - `pkt_txt` is trailing text only, without the leading `:`, or `pkt_empty`.
  - `irc_params[0]` is preloaded with `pkt_par` when a first param exists.
  - `irc_param_count` starts at `1` only when `pkt_par` exists.
  - `irc_params_dirty` is set only when `pkt_rest` is non-empty.
- Trailing text starts only when `:` is at the start of the params tail or immediately follows a separator space. Do not treat `host:port` as trailing.
- After `split_next_param()` has isolated `pkt_par`, the remaining tail can detect trailing text with `rest[0] == ':'` or the forward pattern `" :"`; avoid backward pointer checks in this hot path.
- `irc_params_ensure()` must be idempotent: clear `irc_params_dirty`, then append-tokenize `pkt_rest` from the current `irc_param_count` up to `IRC_MAX_PARAMS=10`.
- `_tokenize_params` appends into `_irc_params + _irc_param_count * 2`; it must not overwrite the preloaded `irc_params[0]`.
- Parser invariant: `irc_params_dirty=1` means `pkt_rest` is non-empty, so `irc_params_ensure()` does not need its own `*pkt_rest` guard. `_tokenize_params` still returns cleanly on empty input as a defensive no-op.
- Treat `_tokenize_params` as parser-internal: call it through `irc_params_ensure()` so the dirty flag, preloaded first param, and append count stay coherent.
- Keep display sanitization after trailing isolation and before dispatch, with the existing hot-path skip for `353/366`.

## Handler Contract

- `PRIVMSG`/`NOTICE`: target is `pkt_par`; only `#` and `&` are channel targets. Unknown channel target returns instead of rendering in the active window. No-trailing fallback may use `irc_param(1)`, but only as a rare compatibility path.
- `pkt_txt` is never `NULL`, and `irc_param()` returns `""` rather than `NULL`; do not add redundant null checks in hot handlers.
- `CAP`: inspect params with `irc_param()`, not raw tail scanning.
- `MODE`: target is `pkt_par`; modes start at `irc_param(1)` and mode args at `irc_param(2+)`.
- `JOIN`/`PART`: use `pkt_par` as the channel, with `pkt_txt` only for legacy trailing-channel forms.
- Numeric handlers and any direct `irc_params[]` loops must call `irc_params_ensure()` first.
- Short fixed-prefix compares inside tokenized params, such as numeric 005 `NETWORK=`, are read-only and safe only because params live inside NUL-terminated `rx_line`. They may read across a token NUL and fail benignly; do not reuse that pattern for writes or for pointers outside `rx_line`.
- Fallback render may print `pkt_par`, `pkt_rest`, and `pkt_txt`, but it must keep timestamp/wrap behavior, the lowercase fragment filter, and the no-prefix unknown-command guard.
- `pkt_cmd` is already NUL-terminated by the command split, so fallback lowercase scanning only needs `while (*p)`, not a secondary space check.

## Applies To

- `src/irc_handlers.c`
- `src/spectalk.c` `irc_params_ensure()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_tokenize_params`
