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
- In `/server host[:port| port]`, mutating the transient `args` buffer at the
  separator and then copying the host with `st_copy_n()` is smaller than
  pointer subtraction plus `memcpy()`. Keep `sep` mutable, but keep the two-step
  `port = sep + 1; skip_spaces((char *)port)` form.
- `remove_channel()` already calls `draw_status_bar()` before returning; do not
  add a second redraw after it unless the caller has changed state again.
- Active non-query slots above index 0 are real joined channels in the current
  slot model (`add_channel()` uses `CH_FLAG_ACTIVE`; `add_query()` adds
  `CH_FLAG_QUERY`; server/status is slot 0). In count scans and `/part` send
  gating, checking `CH_FLAG_ACTIVE | CH_FLAG_QUERY` can replace redundant
  `#`/`&` name-prefix probes when the code already excludes slot 0.
- In `h_mode()`, testing the sign first in the ban/unban path is smaller than
  a separate non-empty guard: `(mode_text[0] == '+' || mode_text[0] == '-') &&
  mode_text[1] == 'b'`. The empty-string case is still false by short-circuit.
- In `cmd_join()`, removing the local `lookup_buf[22]` stack array can shrink
  if the no-prefix path uses `search_pattern` only as transient storage and
  preserves the old bound exactly: `st_copy_n(..., 20)` plus byte `21` forced
  to NUL. Do not switch this path to the full `SEARCH_PATTERN_SIZE`; that would
  change how long no-prefix channel names are sent.
- A tiny helper can beat repeated two-call error blocks when both strings and
  attributes are identical. Current accepted case: `err_maxwin()` for
  `cmd_join()` and `cmd_query()`. This does not contradict the rejected
  static-const dedup for `Use /close first.`; helper/code shape is different.
- Overlay C exit paths should call resident `reset_rx_state()` instead of
  re-emitting `rb_head = 0; rb_tail = 0; rx_pos = 0` when the overlay has
  already clobbered the ring buffer. Accepted cases include
  `overlay/spectalk_ovl3.c` (`-55B`), `overlay/spectalk_ovl4.c` (`-36B`),
  `overlay/spectalk_ovl5.c` (`-37B`), and `overlay/spectalk_ovl.c`
  (`-73B` across help/banner/windows/theme exits).
- For fixed small table scans where the pointer still advances from slot 0,
  changing the `uint8_t` loop counter from count-up compare to countdown
  (`i = MAX; i != 0; i--`) can shrink SDCC output, but measure it. Current
  accepted case: the two `friend_cmd_ovl()` scans saved only `2B`.
- For 32-byte overlay table scans that also need the slot number for display,
  a moving pointer plus a separate sequential display digit can beat repeated
  `base + i * 32` source code even with resident multiply-by-32 copt helpers.
  Current accepted case: `status_render_ovl()` channel rows saved `19B` while
  preserving labels `0..9` and display order.
- The same pattern can still grow if the original loop already uses a moving
  pointer and the old counter also serves as the display index. In
  `overlay/spectalk_ovl.c`, adding a separate `idx` to make
  `windows_render_ovl()` count down grew `SPCTLK1.OVL` by `+6B`.
- For two-dimensional fixed-width overlay tables, a moving pointer can still
  beat repeated `array[i]` references even when the table is small. Current
  accepted case: `overlay/spectalk_ovl5.c` ignores scan uses `ign += 16` and
  saved `3B`; changing the counter to countdown added another `2B` with the
  same visible order.
- In overlay switcher code, tiny pointer rewrites are highly layout-sensitive:
  line-buffer pointer clear and attribute-span pointer fill shrank, but pointer
  versions of the name-copy loop and `sw_flags_snap` snapshot grew badly.
- Countdown loop rewrites are not automatically useful. In
  `overlay/spectalk_ovl.c`, changing the 32-byte banner separator clear loop to
  count down measured flat (`0B`), so the original count-up loop was kept.
- Sequential pointer rewrites for fixed VRAM/attribute stores can be neutral.
  In `overlay/spectalk_ovl.c`, replacing the explicit badge writes to
  `0x5800/0x5820 + 27..31` with `a1/a2` post-increment pointers measured flat
  (`0B`), so the clearer fixed-address stores were kept.
- Do not count edits to sources that are not in the active build graph. In the
  2026-05-10 final pass, `overlay/globe_render.asm` still existed but SPCTLK2
  was built only from `overlay/earth_about_render.asm`; changing it measured
  `0B` and was reverted.

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
  path preserves them in the topic text sent after `:`. Gemini
  SPECTALK-CMD-02 was rejected for this reason.
- After `cmd_topic()` has sent `irc_send_cmd2()`, the temporary separator NUL
  in the command buffer does not need to be restored. This keeps the first-space
  contract above while dropping dead cleanup code.
- Removing dead-looking NULL checks can still grow badly with current SDCC/BPE
  layout. Measured rejection: simplifying the early guard in
  `h_privmsg_notice()` grew TAP by `+107B`.
- Do not replace an 8-bit counted short text loop with a pointer-end loop just
  because it looks more Z80-friendly. In `h_numeric_322_352()`, the pointer-end
  version grew the build; keep the `uint8_t len < 20` loop.
- `find_query()` already returns only query slots or `-1`, so rechecking
  `channels[idx].flags & CH_FLAG_QUERY` after `idx > 0` is redundant and
  measured smaller when removed.
- Do not apply the active-non-query shortcut to display formatting. Prefix
  checks in tab width/switcher labels intentionally strip `#`/`&` from visible
  names and are not slot-type tests.
- Do not replace `REQUIRE_CHAN()` or the current-channel `cmd_topic()` fallback
  with `current_channel_idx`/`chan_flags` unless re-measured on a new layout.
  On 2026-05-08, the macro probe grew TAP by `+41B`; the topic fallback alone
  grew by `+6B`. The existing `IS_CHAN_PREFIX(irc_channel[0])` shape is smaller
  in this SDCC layout.
- Treat scheduler arithmetic rewrites as behavioral changes, not pure shrink,
  if they alter maximum parse work per main-loop pass. The measured
  `max_lines = 6 + ((uint8_t)(backlog >> 8) << 3)` rewrite was rejected and
  reverted because it raised the high-backlog cap from 32 to about 62 lines.
- Do not cache `rx_line[0]` into a local inside `process_irc_data()`'s SNTP wait
  loop without re-measuring. In the 2026-05-08 inline-wrap layout it grew the
  build instead of shrinking it.
- Do not simplify the trailing-text parser guard from `p > pkt_txt && p[-1] ==
  ' '` to `p > pkt_txt` without re-measuring. It is logically redundant, but in
  the 2026-05-08 inline-wrap layout the emitted code grew.
- Do not replace `history_add()`'s bounded `uint8_t` byte-copy loop with
  `memcpy()` without re-measuring. In `codex/size-opt-progressive-gemini` it
  grew TAP `35465B -> 35479B` (`+14B`) despite `memcpy()` already existing in
  the binary.
- Do not replace the `cmd_connect()` registration abort-trap `line[0]`/`line[1]`
  and `rx_line[0]`/`rx_line[1]` checks with raw `*(uint16_t *)` magic compares
  just because `wait_for_connection_result()` uses that shape. In
  `codex/size-opt-progressive-gemini`, SPECTALK-CMD-03 measured exactly flat:
  TAP `35442B -> 35442B`.
- Do not route `/close` directly to `cmd_part()` under a SAFE shrink pass. It
  removes code but changes user-visible error text for Server/inactive-window
  cases; keep it as explicit behavior tradeoff only.
- Do not assume that C-level loop-invariant extraction around ZX VRAM addresses
  shrinks under SDCC. In `overlay/spectalk_ovl3.c`, moving `blit_logo()`'s
  pixel `row_base` into a local that increments by `0x100` grew `SPCTLK3.OVL`
  by `+57B`, and changing the attribute fill to a moving `attr += 32` pointer
  grew by `+7B`. Keep the original expression form unless a hand-ASM rewrite is
  being measured.
- Do not duplicate a call across branches just to pass a literal. In
  `overlay/spectalk_ovl5.c`, passing `"RTC"` directly to `cfg_item()` grew by
  `+15B` when it split the shared `cfg_item(cl_tz, buf)` tail; the exact
  literal-plus-ternary proposal still grew by `+2B`. Keep the branchy buffer
  fill plus shared call shape there.
- Do not assume a mutable static string is smaller than local byte stores in an
  overlay. In `overlay/spectalk_ovl.c`, changing the help title from a local
  `title[12]` filled procedurally to `static char title[] = "Help 0/0"` plus
  two digit stores grew `SPCTLK1.OVL` by `+30B`.
- Do not rewrite `overlay/switcher_ovl.c`'s tab-name copy loop to a local
  `char *dst` countdown form without a new measurement; it grew `SPCTLK6.OVL`
  by `+91B` in the 2026-05-10 layout. Likewise, the `sw_flags_snap` pointer
  snapshot grew by `+82B`; keep the indexed snapshot expression.

## Applied In

- `src/irc_handlers.c` search result and WHOIS numeric handlers
- `src/irc_handlers.c` `h_quit()` one-channel count scan
- `src/user_cmds.c` `cmd_nick()`
- `src/user_cmds.c` `cmd_part()`
- `src/user_cmds.c` `cmd_join()`
- `src/user_cmds.c` `cmd_connect()`
- `src/user_cmds.c` `cmd_topic()`
- `src/user_cmds.c` `cmd_query()`
- `src/spectalk.c` `esp_init()`
- `overlay/spectalk_ovl3.c` overlay command cleanup and friend scans
- `overlay/spectalk_ovl4.c` status channel scan and config-save cleanup
- `overlay/spectalk_ovl5.c` config render cleanup and fixed-width list scans
- `overlay/switcher_ovl.c` switcher render C micro-shrinks
