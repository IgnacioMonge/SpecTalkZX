# Aggressive Z80 Proposal Triage

External Z80 byte/T-state ideas are candidates, not fixes. Classify them against the real target before changing resident code.

## Rule

- Check target first: current SpecTalkZX is a 48K/divMMC/esxDOS build with ROM IM1. Low-memory `RST` slots are not free call gates; `RST 8` belongs to esxDOS and `$0038` belongs to IM1 frame handling.
- Treat SP hijacking as a special-case primitive. It is acceptable only in fully bounded routines that save/restore `SP`, run with interrupts in a known state, make no calls, and prove the copied/cleared address range. Existing `_cls_fast()` is the model; generic runtime UART/RX buffering is not.
- `POP`/`PUSH` stack tricks move memory through stack RAM. They do not read bytes from UART ports, and they corrupt the C stack if any interrupt, ROM call, esxDOS call, or nested helper runs while `SP` is borrowed.
- Opcode-overlap byte skippers are valid only at private hand-ASM labels when flags are dead after the skipped instruction and the synthetic opcode cannot confuse an exported ABI, debug trace, or later fall-through edit.
- SMC state operands require a coherent update plan for every read site. If a flag has multiple readers, every writer must patch all immediate operands or keep a backing variable, often erasing the byte win.
- Tail merging should use named local tail labels with proven stack shape and register liveness. Do not rely on compiler/linker function ordering or unlabeled "Tetris" layout.
- New peephole rules must use the project's z88dk `src/spectalk_copt.rul` syntax, not standalone SDCC `--peep-file` syntax, and must prove removed registers are dead. Rebuild after each rule.

## C/ASM Bridge Checks

- Do not reserve alternate registers globally. Current code already uses `EXX` for cdecl cleanup wrappers and glyph lookup scratch, and uses `EX AF,AF'` for local flags. A permanent `BC'/DE'/HL'` hardware context would conflict with those contracts unless every `EXX` path is rewritten or wrapped.
- Mailbox/global-argument patterns are valid when one hot call path repeatedly pays stack ABI overhead and the mailbox lifetime is local. They are not free: each C store has code size, and persistent mailboxes need real BSS or a proven transient scratch range.
- `$5BC0..$5BFF` is not a generic mailbox. It is transient printer-buffer scratch for glyphs, `print_line64_fast`, BPE return stack, and word-wrap state, and it is not stable across render or esxDOS-facing paths.
- Dense `cmd_id * 2` jump tables only fit dense ids. IRC numeric/message dispatch is sparse and hot-path ordered, while user command dispatch starts from strings and already uses a function pointer table after lookup.
- Broad copt rewrites that remove `ld a,(hl)` must prove `A` is dead after the replacement. copt has pattern context, not global liveness, so add enough trailing context or keep the rule local.

## CRT And Scratch Checks

- Treat z88dk CRT overrides as map-measured switches, not assumptions. This project already disables malloc/stdio heap paths and stdio; `CRT_ENABLE_COMMANDLINE=0` must show a TAP or map delta before being kept.
- Do not move `REGISTER_SP` above the project's documented stack top. High RAM above `$FF58` is live UDG-backed storage for friends, away message, and NAMES target, so `$FFFF` is not a free stack top.
- Do not disable BSS initialization unless the exact CRT startup path is proven. The current custom `code_crt_init` zero-fill is part of the resident startup contract and includes the Z80 `LDIR` size-one guard.
- Buffer aliasing needs real symbol evidence. If the proposed buffer does not exist, or if the candidate alias has a longer-lived protocol role, reject the overlap instead of forcing a memory trick.
- `search_pattern` is scratch; `autojoin_channels` is persistent across connection handshake. Do not alias them unless restored-session JOIN replay is redesigned.
- Indexed UI message vectors are profitable only when many call sites reuse a small message set. For one-off strings, a two-byte table entry plus dispatcher usually cancels the callsite pointer saving.
- Prefer shared `S_` constants over a resident `ui_msg(index)` dispatcher when the same literal is repeated only a few times. Existing BPE-safe constants can remove the duplicate payload without adding table dispatch code.

## Current Accepted Shapes

- Bounded `CPIR` strlen can compute an 8-bit length from `C` with `CPL` only if the scan limit remains `BC=0x0100`. The unbounded `BC=0` variant is rejected because a missing terminator would scan 64K.
- After a bounded `CPIR` from `BC=0x0100`, `B` is always zero on exit, so `LD H,B` is a valid one-byte high-byte clear for the uint8 return.
- `LDIR`/`LDDR` do not modify `IY`; do not preserve `IY` in leaf ASM helpers that only use main registers and block-copy instructions.
- The badge dither triangle must write before `SCF/RLA`, yielding `00,01,03,07,0F,1F,3F,7F`. A report claiming the first byte is `01` is stale against current code.

## Current Rejected Shapes

- Low-byte-only `ChannelInfo` indexing from a 256-byte-aligned `_channels` base fails for `MAX_CHANNELS=10`: slots 8 and 9 cross into the next page. Any page-aligned variant must handle the high-byte carry and justify the BSS padding cost.
- A direct ASM `st_stricmp` entry may be worth prototyping for `_is_ignored`, but do not merge it unless the new entry/exit shape proves a net resident-size win and preserves the current C cleanup wrappers.
- `REGISTER_SP=0xFFFF` is rejected for the current memory map because the stack would collide with high-RAM aliases.
- Generic `CRT_INITIALIZE_BSS=0` is rejected until the generated CRT path is audited against the custom startup zero-fill.
- Aliasing restored-session channel storage over transient line/search buffers is rejected because autojoin replay needs data after those scratch buffers may already have been reused.
- The accepted subset of the message-vector idea is shared constant pooling plus BPE-safe `SB_` renaming, not an index dispatcher.
- File-local C-to-ASM rewrites are acceptable when the fastcall contract is exact and `make` proves the byte win. Current accepted examples in `user_cmds.c`: `cut_at_space()`, `split_at_space()`, and `pool_nth()`.
- C "cache this global byte in a local" suggestions are not automatically wins with SDCC. Current rejected example: caching `rx_line[1]` in `wait_for_connection_result()` grew TAP by 21 bytes.
- Apparent dead `static` constants may be overlay ABI exports in this SCU build; verify `gen_overlay_defs.py` and overlay imports before deleting them.
- Parser helpers that combine split and skip can win when they preserve exact in-place NUL semantics. Current accepted examples in `irc_handlers.c`: `isolate_nick()` and `split_next_param()`.
- Generic copt wildcards are constrained by z88dk syntax; unconstrained end-of-line captures such as `%0call%1%2` are rejected by copt. Use exact rules only after seeing generated ASM.
- Protocol arithmetic specializations must still be measured. Current rejected example: inline IRC 3-digit numeric parsing grew by 331 bytes versus calling the existing `str_to_u16()` ASM helper.

## Applies To

- `src/spectalk_copt.rul`
- `asm/divmmc_uart.asm`
- `asm/spectalk_asm/*.asm`
- `asm/overlay_loader.asm`
- overlay entry ASM helpers
