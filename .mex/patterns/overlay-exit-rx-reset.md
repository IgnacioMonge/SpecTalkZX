# Overlay Exit RX Reset

When overlays are loaded into `ring_buffer`, the shared overlay-exit path must clear any parser/ring state before control returns to normal IRC processing.

## Rule
- `overlay_exec()` drains UART immediately before loading an overlay, then overwrites `ring_buffer` with overlay code. After a successful load it must collapse the ring with `rb_tail = rb_head`; otherwise normal overlay-mode IRC processing can parse overlay binary as server input.
- If that load-time collapse discards any ring bytes, or a partial `rx_line`/overflow was already live, set `rx_overflow=1` so the next tail fragment is discarded until newline. If no data was discarded and no partial line was live, keep `rx_overflow=0` so low-traffic overlays do not swallow the next valid server reply.
- The collapse can be done before the discard comparison: load old `rb_head`/`rb_tail`, store `rb_tail = rb_head`, then clear carry and `sbc hl,de` against the old tail. `ld a,(_rx_overflow)` preserves the Z flag from `sbc`, so the ring-discard predicate can be folded into `A` without a temporary `B`.
- In `_overlay_exec`, the looked-up overlay entry address lives in `DE` after validation. Preserve it before any RX cleanup that reuses `DE`; otherwise `jp (hl)` will jump to a clobbered pointer and reboot/crash as soon as the overlay is entered.
- `_overlay_exec` is a callee-cleanup trampoline. After restoring `IX` and popping the caller return address, prefer `pop bc` to remove the two byte-sized params instead of two `inc sp`; `BC` is caller-saved and the overlay entry receives no live `BC` contract.
- `_overlay_call` protects the looked-up entry pointer on the stack while bounds-checking it. On the success path, pop that protected pointer directly into `HL` before `jp (hl)`; the failure path must still pop and discard it before `ret`.
- Reset `rx_pos` and `rx_overflow` on overlay exit or overlay-load failure.
- When a shared exit clears multiple adjacent 16-bit state variables to zero, prefer `xor a / ld l,a / ld h,a / ld (nn),hl` over byte-pair stores, but only while `A=0` is still needed for byte flags and no live `HL` value exists yet.
- Collapse the ring to empty with `rb_tail = rb_head` so the main loop never parses overlay bytes as IRC data.
- Prefer doing this in the shared ASM exit helper (`overlay_exit_full`) so both normal exits and load-failure paths inherit the cleanup.
- If the overlay was displayed while IRC processing continued in the background, sample `rx_pos`/`rx_overflow` before calling the shared helper and re-arm `rx_overflow=1` afterwards when needed; otherwise the helper cuts the head off the current line and the parser later prints the tail as `>< ...`.
- For repeated user-facing exits, prefer a tiny C wrapper around `overlay_exit_full()` instead of open-coding the same preserve-discard sequence at every call site.
- For cold command/display overlays that already overwrite the ring buffer, prefer calling resident `reset_rx_state()` from each C entry over repeating `rb_head = 0; rb_tail = 0; rx_pos = 0`; this also clears `rx_overflow` and measured smaller in SPCTLK1/3/4/5/6 overlay exits.

## Applied In
- `asm/overlay_loader.asm` `_overlay_exec`
- `asm/spectalk_asm/10_core_helpers.asm` `_overlay_exit_full`
- `src/spectalk.c` `overlay_exit_maybe_discard()`
- `overlay/spectalk_ovl.c` help/banner/windows/theme entries
