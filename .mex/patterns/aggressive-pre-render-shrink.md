# Aggressive Pre-Render Shrink

Before spending resident bytes on demoscene-style render/scroll experiments, recover slack with measured aggressive shrink passes and keep rejected byte economics visible.

## Accepted Results

- Removed dead `print_str64_bpe()` after full-tree grep confirmed no real callsites: `-160B` resident.
- Replaced C `main_hline()` with fixed resident ASM separator draw: `-15B` resident.
- Moved `fmt_buf[8]` to Printer Buffer tail `$5BE9-$5BF0`, leaving `$5BE7-$5BE8` for `main_print_wrapped_ram()` scratch: `+8B` BSS slack with no TAP growth.
- Replaced C `draw_cursor_underline()` with resident ASM preserving attr forcing, scanline 0/7 clearing, and `caps_lock_mode XOR key_shift_held()` cursor placement: `-45B` resident.
- Replaced the single `main_run_u16()` use in `search_render_index()` with local decimal formatting and removed `main_run_u16()` plus `ensure_attr_alignment()`: `-123B` resident. This depends on the callsite already setting `ATTR_MSG_SYS` before the index.
- Replaced C `put_char64_input_cached()` with resident ASM preserving row/column fallback, input-cache checks, real VRAM attribute validation, and final `_print_str64_char()` rendering: `-78B` resident.
- Current build after this batch: `35626B` trimmed / `35706B` TAP / `606B` BSS slack, overlays unchanged at `1396/2037/1593/1986`. After the separate `/msg` query-activation fix, the current branch build is `35659B` trimmed / `35739B` TAP / `573B` BSS slack.
- Moved `/channels` listing to SPCTLK1 entry 2 with a small resident wrapper preserving partial-RX discard: `-184B` TAP/resident net. SPCTLK1 grew to `1653B`, still below 2048B.
- Moved `/theme` name/status text to SPCTLK1 entry 3 while keeping `apply_theme()` resident to avoid nested overlay self-overwrite: `-43B` net. The changed-theme path uses `overlay_call(3)` after `apply_theme()` has loaded SPCTLK1 for the banner.
- Replaced C `main_print()` with resident ASM preserving overlay suppression, short-line `print_line64_fast()` path, slow `main_puts()` path, wrap-indent reset, and final newline: `-38B`.
- Shrink-only build after these follow-ups: `35394B` trimmed / `35474B` TAP / `838B` BSS slack, overlays `1803/2037/1593/1986`.
- HW caught one missing C-level contract in the ASM `main_print()` rewrite: BPE-compressed strings must not use the short-line fast path. The fixed scan now forces any byte `>=0x80` through `main_puts()`, costing about `+7B` and producing the current render/scroll build: `35480B` trimmed / `35560B` TAP / `752B` BSS slack.
- HW later showed corrupted channel-name rendering in the switcher/status path (for example `#Libera`). To preserve stability, `print_str64()`, `reset_all_channels()`, `remove_channel()` defragmentation, and the general `_tokenize_params()` contract were restored to the safer C/general implementations. HW then confirmed the channel names render correctly again. Current build after the rollback plus delayed autojoin is `35676B` trimmed / `35756B` TAP / `555B` BSS slack.

## Rejected Results

- `_print_line64_fast()` empty-pair fast path compiled but cost `+19B` in the first structure and `+36B` in the left-blank-only probe. It may become worthwhile only after a different structural rewrite.
- Fixed-address `cfg_vp` saved 2B BSS but grew CODE/TAP by about 26B; keep it as normal BSS.
- Direct `notif_draw(15, "- MORE: ANY KEY | BREAK: CANCEL -", ATTR_MSG_SYS)` in `pagination_pause()` grew by `+4B`; keep the shared `notif_center()` call.
- `notif_center()` cannot be deleted because overlays import/call it.
- ASM `check_status()` saved only `4B` while touching command precondition flow; keep the C implementation.
- `theme_get_name()` was not simply dead, but its only user was `/theme`; moving the name/status display to SPCTLK1 removed the resident pool/helper safely. Do not call `apply_theme()` from overlay code, because it loads SPCTLK1 for the banner and would overwrite the running overlay.
- ASM `print_str64()` is rolled back. The byte win was too small for a function that renders stack-built and BSS-built UI buffers; keep the C wrapper unless a pixel-perfect HW test covers status, switcher, clock, overlays and odd/even columns.
- ASM `reset_all_channels()` and `channel_defrag_from()` are rolled back. Fixed-stride `channels[]` movement is plausible, but corruption in real UI makes the C struct assignment/memset path the stable contract for now.
- The narrowed `_tokenize_params()` is rolled back. Parser generality only cost a small number of bytes and is not worth risking IRC numerics, JOIN/PART/KICK, and trailing-param handling.

## Hardware Test Focus

- Overlay text paths: ABOUT, CONFIG, WHAT'S NEW, STATUS, all use `print_str64()`.
- Startup/info separator lines from `main_hline()`.
- Input cursor at columns `0`, `1`, `62`, `63`, across both input rows.
- Caps Lock and Symbol/Caps Shift cursor placement: overline vs underline.
- Config load/save path, to confirm `fmt_buf` scratch does not overlap file I/O-sensitive lifetimes.
- Search/list rendering that prints indices: `1.`, `2.`, `10.`, and `100.` if available, checking number/punctuation attribute and wrapping.
- Reconnect/reset path, including slot 0 `Server`, no stale query/channel state, and status/switcher redraw.
- Closing current, non-current, middle, last, channel, and query windows; verify `current_channel_idx`, navigation history, and switcher state visually.
- IRC parser paths with no params, 1 param, 10+ params, server numerics, and trailing `:text with spaces` in `PRIVMSG`/`NOTICE`.
- Input cache paths: type/edit/delete/history across 0-128 chars, both input rows, prompts `>` and `@`, overlay close/redraw, and cursor columns `0/1/62/63`.
- `/channels` output with 1, several, and 10 active windows; include query, unread and mention markers.
- `/theme 1`, `/theme 2`, `/theme 3`, repeated same-theme command, and invalid `!theme` args.
- `main_print()` users: startup BPE strings (`S_APPNAME`, `S_APPDESC`, `S_COPYRIGHT`), `Type !help...`, help/about/config/status overlays, raw system messages, IRC notices, and long lines that must wrap.
