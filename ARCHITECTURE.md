# SpecTalkZX Architecture

## Build Shape

- Resident C builds as one SCU: `src/main_build.c` includes `irc_handlers.c`, `user_cmds.c`, then `spectalk.c`.
- Keep that order: `spectalk.c` owns globals and must stay last.
- `static` symbols are still visible across included modules; check the whole SCU before renaming or deleting.
- `make` runs the BPE pass before compile and restores source copies after compile. If a build is interrupted, run `make restore_bpe`.
- Overlays are separate links against generated resident addresses from `tools/gen_overlay_defs.py`; `overlay/overlay_api.h` is their ABI surface.

## Resident Vs Overlay

- Resident owns the main loop, UART/ring receive, IRC parser, command table, hot rendering, config state, and small wrappers for cold commands.
- `overlay_exec(ovl_id, entry_id)` drains UART, loads one `SPCTLK*.OVL` payload from `SPECTALK.OVL` into `_ring_buffer`, validates the entry pointer, discards overwritten RX ring bytes, then jumps to the entry.
- `overlay_call(entry_id)` calls another entry in the already-loaded overlay. Use it only while that overlay is still resident in `_ring_buffer`.
- `overlay_call_timed(entry_id)` is the ABOUT-only variant that temporarily enables IM1 so ROM `FRAMES` advances during long DAT/draw work.
- Do not load another overlay from an overlay; it overwrites the running code in `_ring_buffer`.

## STOA Overlay Atlas

- `SPECTALK.OVL` is a variable-length STOA atlas, not eight padded 2K pages.
- Header: magic `STOA`, version `1`, overlay count, header length, then `<offset,size>` pairs.
- Each `SPCTLK*.OVL` must still link under 2048 bytes because it executes from the 2K `_ring_buffer`.
- Keep `SpecTalkZX.tap`, `SPECTALK.OVL`, and `SPECTALK.DAT` from the same build on SD card.

## Overlay Map

- `SPCTLK1`: help, banner, `/channels`, `/theme` message.
- `SPCTLK2`: ABOUT open/close and globe tick; Earth packets use private ABOUT storage, not `overlay_slot`.
- `SPCTLK3`: whats-new, bookmark apply, bookmark save.
- `SPCTLK4`: status screen, config save.
- `SPCTLK5`: config screen, RTC seed, `!tz rtc`, numeric `!tz`.
- `SPCTLK6`: raw UDP NTP fallback, channel switcher render.
- `SPCTLK7`: cold local commands: ignore, pass, local settings, autoaway, friend.
- `SPCTLK8`: bookmark selector render/rows/list/delete/cursor.

## Memory Map And Aliases

- `_ring_buffer = $F500..$FCFF` is 2048 bytes. It is both the UART RX ring and the overlay execution area.
- BSS must end before `$F500`; the Makefile fails if `__BSS_END_tail >= $F500` or the guard is too small.
- `_overlay_slot = _rx_line`; it is a 512-byte scratch buffer and is mutually exclusive with active IRC line receive.
- Overlays that reuse `overlay_slot` must reset/discard RX state according to the caller contract; `!save` has the stricter post-SD-I/O drain gate.
- High fixed RAM: `_ignore_list = $FD00..$FD4F`; stack reserve starts above the fixed area.
- Printer buffer `$5B00..$5BFF`: invalidated input cache `$5B00..$5B7F`; Classic leaves `$5B80..$5BBF` free, while SpectraNext binds XFS state at `$5B80..$5B93` and persistent theme/PM/render-cache bytes at `$5B94..$5BBE`; transient render/BPE/parser scratch stays at `$5BC0..$5BFF`. Classic persistent notification/NAMES/PLF state lives in compiler BSS and must not be moved back into Printer RAM.
- SpectraNext aliases the XFS directory scratch to the disposable CHANS input workspace `$5CB6..$5DB5`, and fixes `user_mode[6]` at `$FD50..$FD55` between the ignore list and stack. The linker-map gate proves every target-only owner and the unchanged 2K ring/512B stack boundaries. A command handler must not read parser arguments in `temp_input` after starting an XFS directory transaction; `READDIR` may overwrite that half of the scratch. Current storage-writing handlers either ignore their arguments or consume them before I/O.
- CHANS workspace `$5CB6..$5DB5`: `line_buffer` and `temp_input`.

## Validation

- Before edits: `git status --short --branch`.
- Required checks for code changes: `git diff --check` and `make NO_COLOR=1`.
- Report TAP bytes, BSS guard/free bytes, individual `SPCTLK*.OVL` sizes, and `SPECTALK.OVL` size. Keep only the current verified baseline in `.mex/ROUTER.md`; Git history owns superseded measurements.
- Docs-only changes are `HW N/A`. Any code, timing, memory, UART, SD I/O, or user-visible behavior change is at least `HW PENDING` until tested on hardware.
