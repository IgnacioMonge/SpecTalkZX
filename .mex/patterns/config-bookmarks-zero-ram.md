# Config Bookmarks Zero RAM

IRC bookmarks and session restore must be config-driven and transient. The project has little resident slack, and the existing config loader already has a safe startup-only buffer window.

## Rule
- Store the default session as plain config text: current `server=`, `port=`, independent `autojoin=0|1`, and one compact channel list `channels=#chan,#other`.
- Parse that channel list only into transient scratch during startup/connect flow, then replay it with existing `JOIN` sending paths after IRC registration reaches `STATE_IRC_READY` when `autojoin` is enabled. The current MVP uses `search_pattern[32]`, so the restored comma-list is intentionally short.
- Save the current session from the existing save overlay by iterating `channels[1..MAX_CHANNELS-1]`, skipping inactive slots and `CH_FLAG_QUERY`.
- Avoid a resident multi-bookmark table. Multiple named server bookmarks should be selected by an overlay or by external config editing, not kept in BSS.
- Do not use `ring_buffer` for runtime bookmark browsing while connected; overlays live there. Use `overlay_slot` in bounded chunks or restrict full config parsing to startup.

## Cost Probe
- Implemented independent `autojoin=0|1` plus `channels=` restore: final build is `36179B` trimmed / `36259B` TAP with `45B` BSS slack. `SPCTLK2.OVL` is `2037B` after simplifying `cfg_item()`; `SPCTLK4.OVL` is `1986B`.
- Rejected compatibility branch: accepting old `autojoin=#chan` as an alias for `channels=#chan` costs about 31 resident bytes, dropping BSS slack to 14B. Do not revive it without a shrink pass.
- Robust `/server` reload of config before TCP connect: about `+338B` resident; does not fit current BSS guard without another shrink pass.
- A production version needs either an ASM/minimal parser or a small shrink pass before adding manual bookmark selection.

## Applied In
- `src/spectalk.c` config loader stores `channels=` transiently and parses `autojoin=`.
- `src/user_cmds.c` exposes `!autojoin` and replays the pending list after `001 RPL_WELCOME`.
- `overlay/spectalk_ovl2.c` shows `autojoin=` and `channels=` in `!config`.
- `overlay/spectalk_ovl4.c` writes active non-query channels during `!save`.
