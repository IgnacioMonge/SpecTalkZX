# SpecTalkZX Router

## Project State
- Date: 2026-04-22
- Recent task: narrowed the ikkle footer `PART` fix to the real failure mode. `S_YOU_LEFT` now stays out of BPE and `notif_cancel_current()` still replaces any active footer message before `You have left ...`; the generic BPE-expanding path inside `notify()` was removed to recover resident bytes.
- Build status: `make` completes successfully in this workspace and produces `build/SpecTalkZX.tap`; the earlier `_SB_COLON_SP` failure was not reproduced in the normal full pipeline.
- Current verified TAP from `make`: `35817B` on 2026-04-22 (`-377B` vs the broader 2026-04-21 footer fix, `-255B` vs `v1.3.7`).
- Current overlay sizes from verified `make`: `SPCTLK1.OVL` 1385B, `SPCTLK2.OVL` 1968B, `SPCTLK3.OVL` 1591B, `SPCTLK4.OVL` 1796B.
- Audit highlight: while `OVERLAY_ABOUT` is active, the main loop suppresses client-originated keepalive/lag PINGs and `overlay_keepalive()` only answers server `PING`; this leaves the long-about disconnect bug still open on servers that expect client activity.

## Patterns
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and any prefix routed through `notify2()` must stay out of BPE unless that builder explicitly expands tokens.
- [`overlay-resident-string-dedup.md`](patterns/overlay-resident-string-dedup.md): overlays should reuse resident exported string constants when the text is identical, to recover overlay headroom without changing UI or growing TAP.
