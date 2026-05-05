# Render/Scroll External LLM Triage

## Context

On 2026-05-03 the user collected independent render/scroll optimization
proposals from DeepSeek, Claude, and Gemini for the current 48K divMMC
SpecTalkZX branch. This file preserves the triage so future work does not need
to re-evaluate the same proposals from scratch.

Scope: resident text rendering, scroll, UI refresh, and adjacent RX throughput
paths that affect perceived IRC responsiveness. This is exploration only; no
code change was requested at triage time.

Current constraints from the review package:

- target is classic 48K/divMMC/esxDOS, not Next-only;
- `make NO_COLOR=1` is the authoritative build;
- BSS margin was extremely tight in the diagnostic build family;
- `ring_buffer` starts at `$F500`;
- historical hardware checks showed the bitmap phase dominates;
- normal output must preserve BPE, overlay suppression, pagination cancel, and
  the 64-column vertical alignment contract.

Current accepted hardware baseline after the 2026-05-04 RX/render pass:
parser-cache in `_try_read_line_nodrain`, stack-blit scroll blocks 1/3/5,
stack-clear row 19, and one resident cooperative drain in `process_irc_data()`
after each handled IRC line. Keep profiling/telemetry removable; do not add
generic drains in `_frame_wait()` or manual `/names`.

2026-05-05 follow-up in `codex/scroll-hotpath`: reject Gemini P1. The stack
blitter destination advance via `SP + 32` assembled and the build shrank, but
HW showed corrupted/repeated `Connecting...` text during scroll. The attribute
copy `src=0x5880`, `dst=0x5860`, `len=512` is overlapping; a 16-byte forward
stack blitter writes into bytes that later chunks still need as source. Keep the
attribute copy on `LDIR` unless a replacement proves overlap safety, such as a
reverse copy or a row-wise algorithm that never overwrites unread source.

Corrected accepted shape: `SP + 32` destination advance is valid for bitmap
scroll blocks, which are non-overlapping. Keep the attribute copy on `LDIR`, use
the generic SP blitter for cross-page bitmap blocks 2/4 with `B=2`, and preserve
`IX` once in `_scroll_main_zone` instead of inside every private blitter call.
This builds at TAP `34920B` and should be HW-tested for row 7/15 boundary
copies before promotion.

Same branch follow-up: pure scroll micro-optimization was not perceptible enough
on HW. The useful prior responsiveness change is deferred wrapped output: normal
IRC wrapped text starts after its exact prefix but the body is rendered one
wrapped line per main-loop pass, with UART drained between passes and
`process_irc_data()` breaking immediately so `rx_line` is not reused while the
pending text pointer still points inside it. Do not defer auto-identify success
NOTICEs that release autojoin; keep them synchronous to preserve visible order
and the existing gate. Current measured build with deferred line-step plus the
corrected scroll cleanup: TAP `35145B`, BSS free `1048B`.

Next scroll-only pickup: replacing the attribute `LDIR` with 32 byte-forward
`LDI` instructions looped over 16 rows is overlap-safe and avoids retrying the
rejected SP attribute blit. It preserves the exact byte copy order but costs
resident bytes. Measured build: TAP `35212B`, BSS free `981B`; expected win is
only about 2.3k T-states per scroll before contention.

The same scroll-only pass can also safely hoist `DI` to `_scroll_main_zone`
entry, remove the unused `BC=512` setup before the `LDI` row loop, and compute
the stack-blitter initial `dest+16` from `DE` with `A`/carry rather than
`push hl / ld hl,16 / add hl,de / pop hl`. Do not hoist the helper's saved SP
blindly: each `CALL` has its own return address on the stack, so the helper
still needs a per-call call-frame restore unless its return mechanism is
redesigned.

Corrected attr SP-blit retry: with the fixed helper shape (`IX` preserved once
by `_scroll_main_zone`, `DI` hoisted, initial `dest+16` from `DE`, next dest via
`SP+32`), the attribute copy can be retried as `HL=0x5880`, `DE=0x5860`,
`B=32`. This is still overlap-safe because each 16-byte destination chunk is
two chunks behind the next unread source. Measured build drops back near the
deferred baseline: TAP `35147B`, BSS free `1046B`. Keep the earlier HW failure
in mind; this variant needs a focused hardware test before trust.

Gemini fast-row deferred path: after line-step deferred output is active, using
`print_line64_fast()` for even `main_col` segments reduces wrapped text render
time before each newline/scroll. Odd columns must fall back to `main_puts()` to
preserve half-cell behavior. Measured cost is large but buys general wrapped
output speed, not only server NOTICE: TAP `35308B`, BSS free `885B`.
HW test reported OK and smoother key repeat under incoming channel text.

Smaller accepted successor: move the deferred line-step body to ASM in
`50_main_output.asm` while preserving the same visible contract and the same
even/odd column split. Export `deferred_wrap_attr` and `deferred_wrap_p` as
resident globals for the ASM helper; keep `deferred_wrap_start()` in C. Measured
build: TAP `35152B`, BSS free `1041B`. This recovers `156B` versus the C
fast-row build while keeping the same responsiveness design. HW feedback:
better.

Next tiny scroll-only pickup: because `_scroll_main_zone()` is only called by
`_main_newline()`, and `_main_newline()` already preserves IX/IY for C callers,
do not preserve IX/IY again inside `_scroll_main_zone()`. Also fill last-row
attributes with 16 `push de` writes from `SP=0x5A80`, sharing the pixel-clear
saved-SP window instead of restoring SP and calling `_fast_fill_attr()`. Measured
build: TAP `35156B`, BSS free `1037B`; output should be pixel/attribute
identical. HW test reported all good.

Rejected SAFE micro after measurement: inlining `smz_cross_block` at the two
cross-page bitmap sites removes one trampoline but duplicates setup. It built
at TAP `35165B` (`+9B`) for only a tiny per-scroll timing win, so keep the
shared helper.

## Apply Now Candidates

Ordered by expected usefulness and applicability.

1. **Register-cache `_try_read_line_nodrain` state** (Gemini)
   - Files/symbols: `asm/spectalk_asm/20_rx_ring_uart.asm`,
     `_try_read_line_nodrain`.
   - Implemented 2026-05-04: `DE` caches `_rb_tail`, `BC` caches the live
     `_rx_line` write pointer, `_rb_tail` is committed only on exits, and
     `_rx_pos` is committed only for empty/partial returns.
   - Expected gain: high under IRC bursts, because the old loop reloaded and
     stored ring/parser state per byte.
   - Risk: medium-low. Hardware must still validate `/names`, long partial
     lines, CRLF handling, and overflow discard.

2. **Rejected: IX/IY inline/cache ring push inside `_uart_drain_to_buffer`** (Gemini)
   - Files/symbols: `asm/spectalk_asm/40_text_numeric_screen.asm`,
     `asm/spectalk_asm/20_rx_ring_uart.asm`, `_uart_drain_to_buffer`,
     `_rb_push`.
   - Tested 2026-05-04 as `DE=head`, `IY=tail`, `IXL=budget`, alternate `AF`
     for the byte, and one `_rb_head` commit on exit.
   - Hardware result: much slower visible line output. The DD/FD-prefixed
     `IX/IY` byte-register instructions outweighed removing `call _rb_push`.
   - Do not revive this variant. Any future drain-inline experiment must avoid
     `IX/IY` in the per-byte loop and be hardware-measured before keeping.

3. **Stack-blit scroll blocks 1 and 5** (DeepSeek, Claude)
   - File/symbols: `asm/spectalk_asm/40_text_numeric_screen.asm`,
     `_scroll_main_zone`, `_scroll_stack_blit_224`.
   - Implemented 2026-05-04: existing stack blit was generalized into
     `_scroll_stack_blit_chunks`; block 1 passes `B=8` (128 bytes), block 3
     passes `B=14` (224 bytes), and block 5 passes `B=6` (96 bytes).
     Cross-page 32-byte blocks stay on `LDIR`.
   - Expected gain: high in the red bitmap phase of scroll.
   - Risk: medium-low. Use `IXL`/`IYL` as the chunk counter; do not use `B` if
     the blit body pops into `BC`. Preserve DI-on-exit, SP restore, and IX.

4. **Stack-clear row 19** (Gemini)
   - File/symbol: `asm/spectalk_asm/40_text_numeric_screen.asm`,
     `_scroll_main_zone`.
   - Implemented 2026-05-04: row-19 pixel clear uses `DE=0`, `LD SP,HL`, and
     16 explicit `PUSH DE` operations per scanline, with SP restored before
     attrs.
   - Expected gain: medium-low, but localized.
   - Correction: push a zeroed 16-bit register pair (`DE=0` in current code),
     not `PUSH AF`; `F` is not guaranteed zero and would write garbage bytes.

5. **Space fast path in `_main_putc`** (DeepSeek, Claude)
   - File/symbol: `asm/spectalk_asm/50_main_output.asm`, `_main_putc`.
   - Implemented 2026-05-04: `main_space_inline` is shared with `_main_puts()`
     and `_main_putc()` uses it only for literal space when `cache_row_y`
     already matches `_g_ps64_y`.
   - Gain: low-medium. It avoids `_print_str64_char` setup for isolated spaces
     on cache hit; cache miss remains conservative and warms the normal path.
   - Cost/risk: `+29B` in the current build step; hardware smoke should include
     prompts, timestamps, wrapped output, and status/input redraws.

6. **Remove temporary `dbg_*` counters after diagnostics** (DeepSeek)
   - Files/symbols: `src/spectalk.c`, `include/spectalk.h`, ASM increments,
     status overlay diagnostic output.
   - Expected gain: small direct speedup, useful resident/BSS recovery.
   - Risk: low once diagnostics are no longer needed. Do not remove while a
     hardware diagnostic run still depends on them.

7. **Remove `plf_str_ptr` RAM scratch with stack-held string pointer** (Gemini)
   - File/symbol: `asm/spectalk_asm/30_rendering.asm`, `_print_line64_fast`,
     `plf_str_ptr`.
   - Implemented 2026-05-04: the non-blank pair path now keeps the next string
     pointer on the stack above the saved screen address, and the writer pops it
     back after using `BC` inside the glyph combine loop.
   - Gain: medium for bulk row text, plus resident shrink (`35864B -> 35851B`
     TAP in the current build).
   - Risk: still needs hardware text-screen smoke testing because
     `_print_line64_fast` has many half-blank, NUL-padding, and two-glyph paths.

8. **Direct 3-byte timestamp writer** (Claude)
   - File/symbol: `asm/spectalk_asm/50_main_output.asm`,
     `_main_print_time_prefix`.
   - Revisit only the narrow form that writes the six timestamp cells directly,
     not the rejected full-row `_print_line64_fast` timestamp path.
   - Expected gain: medium with `timestamps=always`, low with smart timestamps.
   - Risk: low-medium if scoped to the fixed prefix contract.

9. **Coalesce multi-scroll wrapped output** (Claude)
   - Files/symbols: `asm/spectalk_asm/50_main_output.asm`,
     `_main_print_wrapped_ram`, `_main_print_wrapped_clean`,
     `asm/spectalk_asm/40_text_numeric_screen.asm`, `_main_newline`.
   - For long wrapped messages near the bottom, avoid multiple full scrolls for
     one logical message.
   - Expected gain: high for NOTICE/MOTD/topic/whois bursts, but not the first
     patch.
   - Risk: medium-high because word-wrap pre-counting, pagination, and cancel
     semantics interact.

## Potential But Not Now

These may become useful under a larger budget, feature cut, or renderer
redesign. Do not present them as immediate safe patches.

1. **Dual pre-masked left/right font tables** (Claude, DeepSeek)
   - Big text-render win by removing per-scanline masks.
   - Blocked by RAM/DAT cost, roughly another full 576-byte table plus renderer
     refactor. Consider only after moving enough cold resident code to overlays
     or accepting a larger feature tradeoff.

2. **Glyph pointer LUT** (Claude)
   - Replaces the `*6` glyph address calculation with a 96-entry pointer table.
   - Blocked by about +192B BSS/page-alignment and overlaps with the larger
     font-table redesign. Useful only if BSS becomes available and pre-masked
     tables are not chosen.

3. **Screen-base LUT** (Claude, DeepSeek)
   - Possible if BSS becomes available, but the benefit is smaller than stated
     because the hot character path already caches the row and bulk renderers
     compute once per row.
   - DeepSeek's attr-base derivation idea was wrong for Spectrum rows; attributes
     need the correct `$5800 + row*32` mapping, not `screen_high + $18`.

4. **Status diff redraw** (Claude)
   - Could help if user-count/status updates become a measured bottleneck.
   - Blocked by shadow-buffer/helper cost and lower priority than scroll/RX
     throughput. Prefer this over a separate dirty-flag system if status work
     is revisited.

5. **`main_print_clean` / BPE-clean output path** (Claude)
   - Potentially useful for a small hand-picked set of guaranteed clean
     callsites.
   - Avoid broad automatic `bpe_build.py` rewriting unless the proof is strong;
     it must also preserve length and line-start contracts, not only BPE
     cleanliness.

6. **Sliding-origin/logical scroll** (Claude)
   - Theoretical scroll win is huge, but it is not a transparent scroll on 48K.
     Without moving or redrawing pixels, visual order becomes circular/jumpy.
   - Treat only as a future UI redesign, not as a local optimization.

## Fully Rejected

Do not spend more time on these unless the target or product decision changes.

- **Frame-accurate/vblank/IM2 scroll** (DeepSeek): does not make 48K VRAM copy
  free; adds timing/interrupt risk.
- **Hardware vertical scroll / 128K / Next tricks for this branch** (Claude):
  not applicable to current divMMC 48K target.
- **Skip attr `LDIR` when chat attributes are uniform** (Claude): too fragile
  with timestamps, nicks, server messages, and highlights.
- **Interleave attribute scroll with bitmap scroll** (Gemini): does not speed the
  routine and costs complexity for a mostly visual effect.
- **RX scatter-gather rendering directly from ring buffer** (Claude): crosses
  too many ownership boundaries; `ring_buffer` is RX/config/overlay storage.
- **ROM font or moving font to ROM** (Claude): not practical.
- **DeepSeek range-check rewrite `sub 33 / cp 96`** (DeepSeek): buggy for char
  128 and likely no net win.
- **Separate status dirty flags** (Claude): redundant if status diff redraw is
  ever implemented.
- **Old low-byte-SMC cross-page SP blit** (Claude): rejected. The corrected
  shape recomputes next destination from `SP + 32`; do not revive the older
  low-byte-only destination update for blocks that cross a page.
- **Drain interleaved in scroll with `EI`** (Claude): risky around IM1/IY and is
  not a render speedup. If RX proof demands it, only consider polling drains
  between safe phases with interrupts still controlled.
- **Deferred `_main_clear_indent6`** (Gemini): likely leaves stale timestamp/wrap
  margin pixels because not every following output goes through the full-row
  renderer.

## Preferred Future Order

When the user exits exploration and asks for implementation, prototype in this
order unless newer hardware data changes the bottleneck:

1. Start from the clean post-diagnostic baseline: no persistent `dbg_*`
   counters, normal `!status`, `DRAIN_NORMAL=32`, and
   `RX_TICK_PARSE_BYTE_BUDGET=512`.
2. Stop for hardware smoke or fund bytes before adding more render features.
3. Narrow timestamp direct writer only if accepting a much tighter BSS margin.
4. Wrapped multi-scroll coalescing.
10. Narrow timestamp direct writer.
