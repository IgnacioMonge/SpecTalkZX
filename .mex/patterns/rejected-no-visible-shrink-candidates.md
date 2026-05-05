---
name: rejected-no-visible-shrink-candidates
description: Measured no-visible-change shrink ideas that should not be retried without a new compiler or ABI reason.
last_updated: 2026-05-05
---

# Rejected No-Visible Shrink Candidates

Baseline for these measurements:
- Worktree: `C:\tmp\SpecTalkZX-gemini-shrink`
- Accepted state: switcher render in `SPCTLK6.OVL`
- Build: TAP `34927B`, BSS guard `0xF00A < 0xF500` (`1270B` free)

Rejected:
- `AT+CIP` prefix factoring: grew to TAP `34964B` (`+37B`). It preserved UART bytes but split sends and added helper cost, so keep full `AT+CIP...` constants/literals.
- Custom cdecl ASM `st_strchr()` replacing all `strchr()` sites: grew to TAP `34944B` (`+17B`). Current libc/SDCC output is smaller.
- Local ASM fill for `overlay_header()` separator instead of two fixed `memset()` calls: grew to TAP `34932B` (`+5B`).
- `h_logged_in` table entry sharing `h_ignore`: measured neutral (`0B`), so keep the named no-op for documentation unless nearby table/code changes make it free.
- Static const dedup for `Use /close first.`: grew by `+11B`; BPE/literal handling is already better.
- `K_TOPIC`/`K_MODE_SP` literal inlining (`C7`): measured neutral (`0B`), TAP and overlays unchanged, so keep named constants.
- `overlay/spectalk_ovl4.c` `(none)` static const dedup: measured neutral (`0B`), `SPCTLK4.OVL` stayed `2014B`, so keep direct literals.
- `IRC_MAX_PARAMS 10->6` (`D6`): user rejected this tradeoff to keep parser capacity/logic unchanged.

A5 note:
- Boot display strings such as `Checking connection...`, `Config loaded.`, ` Autoconnecting`, ` /server to connect`, and `Tip: /nick Name then /server host` are already classified as screen-only by BPE.
- Fatal pre-DAT strings must remain plain resident text.
