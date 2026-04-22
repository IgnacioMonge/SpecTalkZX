# SpecTalkZX Router

## Project State
- Date: 2026-04-23
- Recent task: fixed the 1-pixel vertical mismatch between command-history redraw and normal input text by making `print_line64_fast()` mirror `_print_str64_char()` alignment (blank top scanline, glyph rows start at scanline 1). Verified with full `make`.
- Latest committed checkpoint: `3c2998e` (`Split ASM modules, shrink renderer, and retry SNTP on connect`) — uncommitted: text/numeric screen shrink + fast-render alignment fix + router/pattern updates.
- Build status: local `make` OK on 2026-04-23. TAP `35794B` (`+22B` vs `35772B` prior baseline, `-132B` vs `35926B` pre-shrink baseline). Overlays unchanged (`SPCTLK1` 1385B, `SPCTLK2` 1968B, `SPCTLK3` 1591B, `SPCTLK4` 1796B). BSS guard 389B free before `ring_buffer`.
- Current verified TAP from `make`: `35794B` on 2026-04-23.
- Current overlay sizes: unchanged (`SPCTLK1.OVL` 1385B, `SPCTLK2.OVL` 1968B, `SPCTLK3.OVL` 1591B, `SPCTLK4.OVL` 1796B).
- Audit highlight: the resident ASM split/rendering shrink/SNTP retry from `3c2998e` remains the structural baseline; the latest follow-up is a SAFE local shrink pass in `40_text_numeric_screen.asm` measured at `-154B` TAP with no overlay growth.

## Resume Here
- The worktree now contains an uncommitted structural refactor: `asm/spectalk_asm.asm` is a thin root and the real code lives in ordered modules inside `asm/spectalk_asm/`.
- Keep include order stable unless you intentionally revalidate layout-sensitive ASM changes; the split was done as a contiguous cut so local `jr` ranges and code placement stay unchanged.
- The pre-split baseline is available in `Development/83_asm_split_baseline_20260422`.
- `make build` now owns the BPE preprocessing step. Use `nobpe` only when you intentionally want the non-BPE path.
- After `make`, sanity-check `git diff` before trusting `git status` on `src/*.c`: the BPE prep/restore path in this workspace can leave source-looking noise even when the real task change is only in ASM.
- Add future routines to the nearest domain module instead of growing the root file again.
- `redraw_input_asm()` is still on the faster bulk path, but the generic-space-path experiment in `print_str64_char()` is now known-bad and should not be revived without a more careful proof for status-bar/attr behavior.
- Applied shrink opts on top of Codex pass: unified `p64_space_*` via mask-in-C (-8B), nested-djnz `_notif_clear` without `ldir` (-4B), and `dbc_mask_2sc` helper for `draw_big_char` top/bot (-9B). All three verified HW-OK on 2026-04-22.
- `40_text_numeric_screen.asm` now prefers `_ld_hl_ix4/_ld_hl_ix6` for resident cdecl loads, treats IXL/IYL booleans as zero/nonzero flags, and reuses preserved counts/pointers across adjacent draw phases instead of recomputing them.
- `print_line64_fast()` must stay visually aligned with `_print_str64_char()`: the per-char path leaves scanline 0 blank and starts glyph data at scanline 1, so the fast row renderer must do the same or recalled/history/redrawn text appears 1 pixel higher than incrementally typed text.
- SNTP retry in `/server` path: up to 3 attempts with ~1s spacing, driven by `!sntp_queried`. Fix for clock-stuck-at-00:00 when ESP8266 NTP isn't yet synced on first query.
- The next rendering/UI changes should be re-measured against the new stable baseline `35772B` TAP / `411B` BSS slack.

## Patterns
- [`asm-root-include-split.md`](patterns/asm-root-include-split.md): keep the resident ASM as a thin ordered root with contiguous domain modules so large refactors stay reviewable without disturbing code layout.
- [`build-target-preprocess-deps.md`](patterns/build-target-preprocess-deps.md): any direct build target must own the preprocessing it needs and track include/module inputs explicitly, leaving bypasses as opt-in targets.
- [`irc-mode-wrapper.md`](patterns/irc-mode-wrapper.md): simple IRC wrappers should prefer passthrough semantics with only the minimum implicit target logic needed for convenience, but query numerics should still produce a visible reply.
- [`help-text-source.md`](patterns/help-text-source.md): command help that ships inside `SPECTALK.DAT` should come from a tracked text source, not from hand-edited local DAT payloads.
- [`notification-builder-helpers.md`](patterns/notification-builder-helpers.md): deduplicate short notification builders by arity, but only keep the helper if `make` proves a net byte win.
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and any prefix routed through `notify2()` must stay out of BPE unless that builder explicitly expands tokens.
- [`disconnect-confirmation-guard.md`](patterns/disconnect-confirmation-guard.md): commands that tear down an active connection should share the same guarded `y/n` confirmation helper instead of open-coding input loops.
- [`local-artifacts-gitignore.md`](patterns/local-artifacts-gitignore.md): local tooling files, backups and build outputs that are not part of the shipped program should be hidden via `.gitignore`, not left as noisy untracked entries.
- [`overlay-about-keepalive.md`](patterns/overlay-about-keepalive.md): if `ABOUT` reuses `ring_buffer`, keepalive must continue without full IRC parsing; accept `PONG` in the lightweight scanner and close the overlay before surfacing timeout UI.
- [`overlay-exit-rx-reset.md`](patterns/overlay-exit-rx-reset.md): overlay exit paths must discard parser/ring state if `ring_buffer` has been reused as overlay code storage.
- [`overlay-resident-string-dedup.md`](patterns/overlay-resident-string-dedup.md): overlays should reuse resident exported string constants when the text is identical, to recover overlay headroom without changing UI or growing TAP.
- [`fast-text-renderer-alignment.md`](patterns/fast-text-renderer-alignment.md): any bulk 64-col row renderer must reproduce the same scanline padding as the per-character renderer, or redrawn text will jump vertically relative to incrementally printed text and the cursor/prompt.
- [`resident-asm-helper-reuse.md`](patterns/resident-asm-helper-reuse.md): in resident hand-written ASM, prefer shared IX load helpers and preserved register reuse over open-coded re-materialization when the callee contract already proves the values are still live.
- [`rendering-hotpath-byte-rules.md`](patterns/rendering-hotpath-byte-rules.md): for rendering hot paths, exploit proven alignment/caching contracts, collapse byte-packed coordinate adds into 16-bit adds, and do not pay `IY` save/restore or cache revalidation that the callee contract already makes unnecessary.
- [`sntp-retry-on-connect.md`](patterns/sntp-retry-on-connect.md): the `/server` connect path must retry `AT+CIPSNTPTIME?` up to 3 times with a delay, because the ESP8266 can return a `1970` placeholder before NTP has actually resolved; retries must use `sntp_queried` as the exit signal, stay inside the AT-command window, and honor `BREAK` across all attempts.
