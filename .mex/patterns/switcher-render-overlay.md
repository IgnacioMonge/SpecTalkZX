---
name: switcher-render-overlay
description: Contract for keeping the channel switcher render body in SPCTLK6 without changing switcher UX.
last_updated: 2026-05-05
---

# Switcher Render Overlay

Resident budget win:
- `switcher_render()` may be a resident stub that calls `overlay_exec(5, 1)`.
- `SPCTLK6.OVL` entry 0 remains raw UDP NTP; entry 1 is `switcher_render_ovl`.
- Do not add a seventh overlay block for this path unless TAP growth is explicitly accepted.

State contract:
- Keep `sw_active`, `sw_dirty`, `sw_sel`, `sw_first`, `sw_count`, `sw_released`, `sw_timeout`, `sw_map`, and `sw_flags_snap` resident.
- Keep switcher key handling in the main loop resident.
- `sw_map` and `sw_flags_snap` still alias `search_pattern[]`; do not use them while pagination/search owns that buffer.
- `switcher_rebuild_map()`, `sw_tab_width()`, and `switcher_close()` remain resident and are exported through `tools/gen_overlay_defs.py`.

UX contract:
- The tab text stays byte-for-byte equivalent: `" N:name "` or `" N:@name "`, with `|`, `<`, and `>` markers unchanged.
- Attribute behavior stays equivalent: selected tab inverse, unread bright, mention flash.
- `switcher_close()` still clears row 2, redraws the separator scanline, and refreshes `last_frames_lo`.

Risk:
- Overlay code runs from `ring_buffer`, so each dirty switcher redraw drains/drops pending UART ring data through the normal overlay loader contract.
- Hardware test after changes must cover EDIT open/release close, LEFT/RIGHT navigation, ENTER select, BACKSPACE part, numeric jump, unread/mention refresh, and IRC traffic while switcher is open.

Measurement:
- 2026-05-05 safe worktree build: TAP `34927B`, BSS guard `0xF00A < 0xF500` (`1270B` free), overlays `1808/1846/1547/2014/2001/1733`, `SPECTALK.OVL=12288B`.
- User hardware test on 2026-05-05 confirmed this build OK.
- 2026-05-10 Gemini switcher C shrink mini-branch: accepted local C rewrites
  reduced `SPCTLK6.OVL 1668B -> 1651B` without changing TAP size. Accepted:
  `sw_ch` macro (`-6B`), attribute-span pointer fill (`-4B`), and line-buffer
  pointer clear (`-7B`). Rejected: name-copy pointer loop (`+91B`) and
  `sw_flags_snap` pointer snapshot (`+82B`).
