# SpecTalkZX Router

## Project State
- Date: 2026-04-22
- Recent task: added a guarded disconnect confirmation for `/quit`. `/quit` now asks `Disconnect (y/n)?` in error/red style before sending `QUIT`, and `/connect` reuses the same helper when already connected.
- Build status: `make` completes successfully in this workspace and produces `build/SpecTalkZX.tap`; the earlier `_SB_COLON_SP` failure was not reproduced in the normal full pipeline.
- Current verified TAP from `make`: `35844B` on 2026-04-22 (`+27B` vs the previous 35,817B build, `-228B` vs `v1.3.7`).
- Current overlay sizes from verified `make`: `SPCTLK1.OVL` 1385B, `SPCTLK2.OVL` 1968B, `SPCTLK3.OVL` 1591B, `SPCTLK4.OVL` 1796B.
- Audit highlight: while `OVERLAY_ABOUT` is active, the main loop suppresses client-originated keepalive/lag PINGs and `overlay_keepalive()` only answers server `PING`; this leaves the long-about disconnect bug still open on servers that expect client activity.

## Patterns
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and any prefix routed through `notify2()` must stay out of BPE unless that builder explicitly expands tokens.
- [`disconnect-confirmation-guard.md`](patterns/disconnect-confirmation-guard.md): commands that tear down an active connection should share the same guarded `y/n` confirmation helper instead of open-coding input loops.
- [`overlay-resident-string-dedup.md`](patterns/overlay-resident-string-dedup.md): overlays should reuse resident exported string constants when the text is identical, to recover overlay headroom without changing UI or growing TAP.
