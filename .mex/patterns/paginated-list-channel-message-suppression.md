# Manual NAMES Dispatcher Filter And Grid

## Context
Manual `/names` reuses the main chat area while IRC channel traffic keeps arriving and the server sends `353` chunks. The raw `353` payload is a long space-separated list, which is hard to read and pays the generic word-wrap path.

## Rule
- Do not fix manual `/names` leaks by adding checks to every event handler.
- While `show_names_list` is active, filter centrally in the dispatcher.
- Allow only `353` and `366` for the NAMES response, plus `PING` and `PONG` so keepalive still works.
- Do not filter automatic background NAMES (`show_names_list == 0`), because those update counts and friend notifications without owning the main area.
- Do not solve this in `main_print()`: the NAMES renderer itself uses `main_print*()`, so the filter must happen before dispatch to normal channel event handlers.
- Keep the normal `pagination_pause()` path. Manual `/names` must remain readable page-by-page, cancellable with BREAK, and able to report incomplete results when ring-buffer pressure forced a discard.
- Render manual NAMES as fixed cells, not as raw wrapped text. `_names_render_grid` packs four 16-column nick cells into `temp_input`, keeps mode prefixes such as `@` and `+`, truncates long nicks with `>`, and paints each row through `print_line64_fast()`.
- Keep this renderer in ASM. The equivalent C helper compiled too large; the ASM version is a mechanical parser/filler and keeps resident slack acceptable.
- `366` must match the active `names_target_channel` before it finalizes manual `/names`; otherwise an unrelated NAMES completion can drop `show_names_list` and let channel traffic leak into the list.
- When manual `/names` finishes or times out, purge stale RX backlog. During `MORE`, old channel traffic may accumulate behind the NAMES response; once `show_names_list` is cleared, that stale backlog would otherwise render below the list.
- Current measured cost after ASM micro-shrink plus closure hardening is about +145B over the dispatcher-only `/names` fix; the first grid ASM implementation was +167B, and the C grid helper was worse.

## Applied In
- `src/irc_handlers.c` `parse_irc_message()`
- `src/irc_handlers.c` `h_numeric_353()`
- `asm/spectalk_asm/50_main_output.asm` `_names_render_grid`
