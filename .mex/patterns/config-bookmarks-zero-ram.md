# Config Bookmarks Zero RAM

IRC bookmarks and session restore must be config-driven and transient. The project has little resident slack, and the existing config loader already has a safe startup-only buffer window.

## Rule
- Store the default session as plain config text: current `server=`, `port=`, independent `autojoin=0|1`, and one compact channel list `channels=#chan,#other`.
- Parse that channel list into `autojoin_channels[64]`, then replay it with existing `JOIN` sending paths after the server reaches the end of its registration burst. `001` only marks IRC ready; restored-session `JOIN` waits for `376` end-of-MOTD or `422` no-MOTD, and for any in-flight auto-identify acknowledgement. The comma-list is capped at 63 bytes plus NUL.
- Keep `search_pattern[]` as volatile scratch for searches, theme overlay state, and `!config` display snapshots. Do not store the persistent restored-session channel list only in `search_pattern`; too many unrelated paths may reuse it before autojoin or before the next save.
- Before rendering `!config`, snapshot active non-query `channels[]` into `search_pattern` with `_snapshot_autojoin_channels()`. The snapshot helper returns `1` when the generated list differs from the previous contents; `!config` uses that to set `config_dirty` so newly joined or parted channels offer save again.
- `SPCTLK4` save must prefer a direct scan of active non-query `channels[]` when connected, then fall back to `autojoin_channels` only if no active channels exist. This keeps disconnected saves from dropping a loaded `channels=` line, but also avoids losing newly joined channels if the transient display snapshot was stale or reused.
- Do not clear `search_pattern` after restored-session autojoin sends `JOIN`; clear the autojoin defer flags instead. Keeping the loaded list lets the later active-channel snapshot compare against the saved config instead of marking a restored, unchanged session as dirty.
- `SPCTLK4` is tight after this robustness fix (`2044/2048` in the 2026-04-25 build); do not add another config-save feature there without first freeing overlay bytes or moving the channel writer to a smaller form.
- Avoid a resident multi-bookmark table. Multiple named server bookmarks should be selected by an overlay or by external config editing, not kept in BSS.
- Do not use `ring_buffer` for runtime bookmark browsing while connected; overlays live there. Use `overlay_slot` in bounded chunks or restrict full config parsing to startup.

## Cost Probe
- Implemented independent `autojoin=0|1` plus `channels=` restore: original MVP build was `36179B` trimmed / `36259B` TAP with `45B` BSS slack. After later shrink/render work and the overlay snapshot-only save path, current build is `35945B` trimmed / `36025B` TAP with `286B` BSS slack. `SPCTLK2.OVL` remains `2037B`; `SPCTLK4.OVL` is now `1958B`.
- Rejected compatibility branch: accepting old `autojoin=#chan` as an alias for `channels=#chan` costs about 31 resident bytes, dropping BSS slack to 14B. Do not revive it without a shrink pass.
- Robust `/server` reload of config before TCP connect: about `+338B` resident; does not fit current BSS guard without another shrink pass.
- A production version needs either an ASM/minimal parser or a small shrink pass before adding manual bookmark selection.

## Applied In
- `src/spectalk.c` config loader stores `channels=` transiently and parses `autojoin=`.
- `asm/spectalk_asm/70_input_lookup.asm` exposes `_snapshot_autojoin_channels()` so resident C can refresh the transient `channels=` display/save buffer without growing SPCTLK2.
- `src/user_cmds.c` exposes `!autojoin`; `src/irc_handlers.c` replays the pending list from the `376`/`422` handshake gate.
- `overlay/spectalk_ovl5.c` shows `autojoin=` and `channels=` in `!config`.
- `overlay/spectalk_ovl4.c` writes the already-refreshed `search_pattern` during `!save`; it must not grow back a separate channel scan unless future callsites bypass `cmd_save()`.
