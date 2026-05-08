# SDCC C Structural Shrink

Use this when reviewing C-level shrink proposals that rely on SDCC codegen
rather than hand-written ASM.

## Rule

- One-use locals that only feed a fixed accessor can be worth removing, but
  measure the final build. Current accepted example: `h_numeric_322_352()`
  calls `irc_param(3)`/`irc_param(5)` directly instead of assigning
  `host_idx`/`nick_idx`.
- Ternary string selection can shrink branchy `if/else main_putc('?')` shapes
  when a reusable string literal already exists and the callee accepts a string.
  Current accepted example: WHOIS 311 prints `*irc_param(n) ? irc_param(n) :
  "?"` style fallbacks through `main_puts()`.
- Preserve const types explicitly when SDCC warns. In `cmd_nick()`, cast the
  mutable `irc_nick` arm to `(const char *)` before passing the ternary result
  to `main_print()`.
- Reuse paid wait helpers only when behavior remains valid. `esp_init()` may use
  `wait_drain(55)` around the ESP `+++` guard silence because it reads/drains
  UART but does not send extra bytes during the required quiet period.
- For `irc_param()` results, pointer NULL checks are dead because the accessor
  returns `""` for missing params. Keep only `!*p` checks in that narrow case.
- Parser globals `pkt_par` and `pkt_txt` are initialized to `pkt_empty`, so
  non-NULL checks are also dead after normal `parse_irc_message()` setup.
- Case-folding yes/no prompts with `(k | 32)` is accepted for ASCII key results
  from `in_inkey()`; BREAK/ENTER/control keys do not become `y` or `n`.
- In `/server`, direct no-separator copy with
  `st_copy_n(irc_server, args, sizeof(irc_server))` avoids a resident
  `st_strlen()` call path while preserving the full destination bound. Do not
  pass `sizeof(irc_server) - 1` to `st_copy_n()` unless one less payload byte is
  deliberately desired.
- `remove_channel()` already calls `draw_status_bar()` before returning; do not
  add a second redraw after it unless the caller has changed state again.

## Guardrails

- Do not assume a ternary is smaller. Build before keeping it.
- Do not remove null checks unless the producer contract is local and stable.
  `irc_param()` returns `""` for missing params, so non-NULL string tests are
  redundant there; raw parser pointers do not have that contract.
- Avoid const-warning rewrites. `skip_spaces((char *)sep + 1)` warns under SDCC;
  the existing two-step `port = sep + 1; skip_spaces((char *)port)` form builds
  cleanly here.
- `cmd_search()` must keep its explicit `SEARCH_PATTERN_SIZE-1` truncation loop
  unless a replacement also truncates the visible `src` token before printing.
  Plain `split_at_space(src)` changes long no-space patterns: the actual stored
  search still truncates later, but the on-screen "Searching..." text does not.
- `cmd_topic()` cannot blindly switch from `strchr()` to `split_at_space()`.
  `split_at_space()` skips spaces after the channel token, while the existing
  path preserves them in the topic text sent after `:`.
- Removing dead-looking NULL checks can still grow badly with current SDCC/BPE
  layout. Measured rejection: simplifying the early guard in
  `h_privmsg_notice()` grew TAP by `+107B`.
- Do not replace an 8-bit counted short text loop with a pointer-end loop just
  because it looks more Z80-friendly. In `h_numeric_322_352()`, the pointer-end
  version grew the build; keep the `uint8_t len < 20` loop.
- `find_query()` already returns only query slots or `-1`, so rechecking
  `channels[idx].flags & CH_FLAG_QUERY` after `idx > 0` is redundant and
  measured smaller when removed.
- Treat scheduler arithmetic rewrites as behavioral changes, not pure shrink,
  if they alter maximum parse work per main-loop pass. The measured
  `max_lines = 6 + ((uint8_t)(backlog >> 8) << 3)` rewrite was rejected and
  reverted because it raised the high-backlog cap from 32 to about 62 lines.

## Applied In

- `src/irc_handlers.c` search result and WHOIS numeric handlers
- `src/user_cmds.c` `cmd_nick()`
- `src/spectalk.c` `esp_init()`
