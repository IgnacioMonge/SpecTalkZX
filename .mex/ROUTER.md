# SpecTalkZX Router

## Project State
- Date: 2026-04-22
- Recent task: implemented and then compacted the `OVERLAY_ABOUT` keepalive fix. The main keepalive sender stays active during `ABOUT`, `overlay_keepalive()` consumes `PONG`, timeout during `ABOUT` closes the overlay before printing/disconnecting, and the merged `PING/PONG` parser recovered most of the first-pass cost.
- Build status: `make` completes successfully in this workspace and produces `build/SpecTalkZX.tap`; the earlier `_SB_COLON_SP` failure was not reproduced in the normal full pipeline.
- Current verified TAP from `make`: `35861B` on 2026-04-22 (`+17B` vs the previous 35,844B build, `-211B` vs `v1.3.7`).
- Current overlay sizes from verified `make`: `SPCTLK1.OVL` 1385B, `SPCTLK2.OVL` 1968B, `SPCTLK3.OVL` 1591B, `SPCTLK4.OVL` 1796B.
- Audit highlight: the long-`ABOUT` disconnect hole is now addressed, and the merged `PING/PONG` scanner reduced the first-pass cost by 64B in this toolchain.

## Patterns
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and any prefix routed through `notify2()` must stay out of BPE unless that builder explicitly expands tokens.
- [`disconnect-confirmation-guard.md`](patterns/disconnect-confirmation-guard.md): commands that tear down an active connection should share the same guarded `y/n` confirmation helper instead of open-coding input loops.
- [`local-artifacts-gitignore.md`](patterns/local-artifacts-gitignore.md): local tooling files, backups and build outputs that are not part of the shipped program should be hidden via `.gitignore`, not left as noisy untracked entries.
- [`overlay-about-keepalive.md`](patterns/overlay-about-keepalive.md): if `ABOUT` reuses `ring_buffer`, keepalive must continue without full IRC parsing; accept `PONG` in the lightweight scanner and close the overlay before surfacing timeout UI.
- [`overlay-exit-rx-reset.md`](patterns/overlay-exit-rx-reset.md): overlay exit paths must discard parser/ring state if `ring_buffer` has been reused as overlay code storage.
- [`overlay-resident-string-dedup.md`](patterns/overlay-resident-string-dedup.md): overlays should reuse resident exported string constants when the text is identical, to recover overlay headroom without changing UI or growing TAP.
