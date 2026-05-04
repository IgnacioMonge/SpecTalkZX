# Resident History/Switcher Shrink

Use this when shaving resident bytes without touching RX scheduling or visible IRC output.

## Rule

- Command history BSS is a valid pressure valve, but keep the execution path untouched: long commands still execute from `line_buffer`/`temp_input`; only the stored history copy is truncated.
- `HISTORY_SIZE=4` stays useful because the ring math is `& 3`. If the depth changes, audit every hardcoded mask and navigation path.
- `HISTORY_LEN=48` is the current measured compromise: it saves resident margin while keeping four recall entries. Document the UX tradeoff as "history stores first 47 chars"; do not imply command input itself is capped.
- Keep a private `history_draft[LINE_BUFFER_SIZE]` for the line being edited before entering history. Do not reuse `temp_input` for this: notification builders and parser/UI paths also use `temp_input`, so a live `notify()` can overwrite the draft and make notification text appear when arrow-down leaves history.
- Consecutive duplicate suppression in `history_add()` is optional UX polish, not correctness. Removing it saved resident bytes because the `st_stricmp(history[last], cmd)` block was larger than its value under pressure.
- For history restore/temp-save, `st_copy_n(..., sizeof(line_buffer))` measured smaller than `memcpy(..., len + 1)` in this build and preserves the NUL-terminated line-buffer contract, but only when the source scratch is private to history.
- In `switcher_render()`, `print_line64_fast()` consumes a fixed 64 logical columns. A stack buffer of `SCREEN_COLS` filled with spaces is enough; do not add `buf[SCREEN_COLS] = 0`.
- Cache `slot = sw_map[i]` when rendering a switcher tab. It shrinks repeated indexed loads and makes using `sw_tab_width(slot)` cheaper than duplicating the width formula.
- CTCP VERSION should reuse `S_APPSHORT` for the response data. Do not carry a second local `"SpecTalkZX"` literal in `irc_handlers.c`.

## Rejected Here

- Pre-capping history `len` before the copy loop grew the current build by about `+23B`; keep the original double-condition loop.
- Copying the switcher tab name with `for (; name[j]; ...)` after `sw_tab_width()` grew by about `+14B`; keep the explicit `nlen = st_strlen(name)` loop.
- Local dedup of `"Banned"` in `user_cmds.c` grew by about `+3B`; BPE already handles that shape better.
- Moving `irc_port` default `"6667"` from the data initializer to runtime `st_copy_n(S_DEFAULT_PORT)` grew by about `+16B`.
- Exporting BPE-managed resident strings such as `S_ON`, `S_OFF`, `S_NOTSET`, and `S_SMART` to C overlays fails as undefined `_S_*` symbols. Keep overlay-local copies unless a non-BPE exported ABI symbol is deliberately added.

## Applies To

- `src/spectalk.c` command history and `switcher_render()`
- `src/irc_handlers.c` CTCP VERSION path
