---
name: resident-channel-reset-ldir
description: Guardrail for fixed-size channel reset rewrites; LDIR may measure smaller but needs hardware proof here.
last_updated: 2026-04-28
---

# Resident Channel Reset LDIR

Direct `LDIR` clears can beat both AoS C loops and C `memset()` call setup for fixed resident arrays.
That is not enough to land them in this project.

Current evidence:

- C `memset(channels, 0, sizeof(channels))` grew TAP by 7 bytes.
- A hand-ASM `_reset_all_channels` prototype that cleared all 320 bytes of `channels`, copied `S_SERVER`, and restored globals measured `-32B`.
- The prototype was still reverted because `aggressive-pre-render-shrink.md` records a prior hardware rollback of ASM `reset_all_channels()`/channel movement after corrupted channel-name rendering in switcher/status paths.

Rule:

- Keep the C `reset_all_channels()` unless the ASM replacement is tested on hardware through reconnect/reset, status bar redraw, and switcher rendering.
- If relanding, the ASM must full-zero the struct array, rebuild slot 0 explicitly, and avoid any channel defrag rewrite in the same change so failures are attributable.
