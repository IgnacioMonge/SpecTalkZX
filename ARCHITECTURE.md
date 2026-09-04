# SpecTalkZX Architecture

## Build Shape

- Resident C builds as one SCU: `src/main_build.c` includes `irc_handlers.c`, `user_cmds.c`, then `spectalk.c`.
- Keep that order: `spectalk.c` owns globals and must stay last.
- `static` symbols are still visible across included modules; check the whole SCU before renaming or deleting.
- `make` runs the BPE pass before compile and restores source copies after compile. If a build is interrupted, run `make restore_bpe`.
- `make next` builds only the native Spectrum Next target and emits `build/SPECTALK.NEX`; it does not build Classic.
- Overlays are separate links against generated resident addresses from `tools/gen_overlay_defs.py`; `overlay/overlay_api.h` is their ABI surface.

## Resident Vs Overlay

- Resident owns the main loop, UART/ring receive, IRC parser, command table, hot rendering, config state, and small wrappers for cold commands.
- On Classic, `overlay_exec(ovl_id, entry_id)` drains UART, loads one `SPCTLK*.OVL` payload from `SPECTALK.OVL` into `_ring_buffer`, validates the entry pointer, discards overwritten RX ring bytes, then jumps to the entry.
- On Spectranext, `overlay_exec()` reads the 64-byte atlas header plus a 256-byte bootstrap into `_ring_buffer`; the bootstrap reuses that open atlas handle, stages the selected payload through the 512-byte slice at `_ring_buffer+512` and copies it into one reusable 4K cartridge SRAM page at Page B (`$2000`). `_overlay_slot` remains intact for overlay arguments. The resident trampoline maps that page only while executing an entry.
- On native Spectrum Next, the NEX holds each overlay in its own 8K page. `overlay_exec()` maps pages 16..23 directly at MMU1 (`$2000`) with no SD read or ring-buffer copy; the final two bytes of each page hold the exact compiled payload length used to reject entry pointers into padding. `SPCTLK2` reserves the preceding 512 bytes (`$3DFE..$3FFD`) as persistent ABOUT packet scratch.
- `overlay_call(entry_id)` calls another entry in the already-loaded overlay. Classic executes it from `_ring_buffer`; Spectranext remaps the current Page B SRAM image; native Next remaps the last MMU1 page.
- `overlay_call_timed(entry_id)` is the ABOUT-only variant. Classic enables IM1 during the entry; both paged targets keep overlay execution under DI and `frame_wait()` briefly restores ROM for each interrupt. Native Next overlay and embedded-DAT trampolines always return with the mainline DI contract intact.
- Do not load another overlay from an overlay; it replaces the active execution image.

## STOA Overlay Atlas

- `SPECTALK.OVL` is a variable-length STOA atlas, not eight padded 2K pages.
- Header: magic `STOA`, version `1`, overlay count, header length, then `<offset,size>` pairs.
- Classic overlays link under 2048 bytes at `_ring_buffer`. Spectranext overlays link under 4096 bytes at `$2000`; its atlas has a fixed 256-byte bootstrap immediately after the 64-byte header. Native Next overlays link under 8190 bytes at `$2000`; the NEX packer expands the atlas into one page per overlay and reserves `$3FFE..$3FFF` for its payload length.
- Keep `SpecTalkZX.tap`, `SPECTALK.OVL`, and `SPECTALK.DAT` from the same build on SD card.
- Native Next uses only `SPECTALK.NEX`; its DAT bytes are embedded behind a two-byte generated length header in pages 24..27.

## Overlay Map

- `SPCTLK1`: help, banner, `/channels`, `/theme` message.
- `SPCTLK2`: ABOUT open/close and globe tick; Earth packets use private ABOUT storage, not `overlay_slot`. Native Next uses reserved page-tail scratch because its directly mapped overlay page persists between calls; the reload-on-call targets may reuse dead entry-0 code.
- `SPCTLK3`: whats-new; on Classic it also owns bookmark apply/save.
- `SPCTLK4`: status and config save; on Spectranext it also owns bookmark apply/save so the XFS replace-write helper is linked only once.
- `SPCTLK5`: config screen, RTC seed, `!tz rtc`, numeric `!tz`; native Next also owns one shared bus/ESP reset pulse used by bounded connection recovery.
- `SPCTLK6`: raw UDP NTP fallback, channel switcher render.
- `SPCTLK7`: cold local commands: ignore, pass, local settings, autoaway, friend.
- `SPCTLK8`: bookmark selector render/rows/list/delete/cursor.

## Memory Map And Aliases

- `_ring_buffer = $F500..$FCFF` is 2048 bytes. Classic also uses it as the overlay execution area. Spectranext uses it only for the transient paging bootstrap and ROM-safe staging, then executes overlays from cartridge SRAM.
- Native Next leaves `_ring_buffer` available for RX and executes overlays at MMU1. Resident code starts at `$5DC0`, so MMU3 (`$6000`) is not a legal overlay slot.
- BSS must end before `$F500`; the Makefile fails if `__BSS_END_tail >= $F500` or the guard is too small.
- `_overlay_slot = _rx_line`; it is a 512-byte scratch buffer and is mutually exclusive with active IRC line receive.
- Overlays that reuse `overlay_slot` must reset/discard RX state according to the caller contract; `!save` has the stricter post-SD-I/O drain gate.
- High fixed RAM: `_ignore_list = $FD00..$FD4F`; stack reserve starts above the fixed area.
- Printer buffer `$5B00..$5BFF`: invalidated input cache `$5B00..$5B7F`; Classic leaves `$5B80..$5BBF` free, while Spectranext binds XFS state at `$5B80..$5B93` and persistent theme/PM/render-cache bytes at `$5B94..$5BBE`; transient render/BPE/parser scratch stays at `$5BC0..$5BFF`. Classic persistent notification/NAMES/PLF state lives in compiler BSS and must not be moved back into Printer RAM.
- Spectranext aliases the XFS directory scratch to the disposable CHANS input workspace `$5CB6..$5DB5`, and fixes `user_mode[6]` at `$FD50..$FD55` between the ignore list and stack. The linker-map gate proves every target-only owner and the unchanged 2K ring/512B stack boundaries. A command handler must not read parser arguments in `temp_input` after starting an XFS directory transaction; `READDIR` may overwrite that half of the scratch. Current storage-writing handlers either ignore their arguments or consume them before I/O.
- CHANS workspace `$5CB6..$5DB5`: `line_buffer` and `temp_input`.

## Validation

- Before edits: `git status --short --branch`.
- Required checks for native Next code changes: `git diff --check` and `make next NO_COLOR=1`. `next-check` runs the shared source/BPE/COPT/storage contracts used by this target; do not add a redundant Classic build when shared Classic behaviour was untouched.
- `check_memory_layout.py --platform next` owns resident/BSS/stack boundaries; `test_next_nex_image.py` separately owns native bank order, page payloads, entry bounds and embedded DAT layout.
- Report TAP bytes, BSS guard/free bytes, individual `SPCTLK*.OVL` sizes, and `SPECTALK.OVL` size. Record the current verified release baseline in `CHANGELOG.md`; Git history owns superseded measurements.
- Docs-only changes are `HW N/A`. Any code, timing, memory, UART, SD I/O, or user-visible behavior change is at least `HW PENDING` until tested on hardware.
