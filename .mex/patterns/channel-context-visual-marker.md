name: channel-context-visual-marker
description: Contract for the channel context visual experiment.

# Channel Context Visual Marker

Problem: with several IRC windows open, the shared main transcript can be hard
to parse immediately after changing the active channel.

Accepted experiment shape:

- Print exactly one visual divider from `switch_to_channel()` after the active
  slot changes.
- Divider layout: `HH:MM` in Ikkle-4 on the left, a 1-pixel horizontal line
  on scanline 4 through the middle, and the current channel/query/server name
  in Ikkle-4 right-aligned.
- Render leading `#` or `&` in channel names as a blank, preserving spacing
  while avoiding the noisy hash glyph: `TIMESTAMP ---- CHANNEL`.
- Server labels also start with one blank so the separator line does not touch
  `SERVER`.
- Query labels do not draw the leading `@`; replace it with one blank and then
  draw the nick directly.
- Do not patch the shared Ikkle font table for query `@` in this experiment
  without a hardware-boot gate; the first cosmetic `@` probe was reverted after
  a `Bytes: SpecTalkZX` start failure report.
- Use bright ink on the normal main background. Pick from all six bright inks
  using a case-insensitive cleaned-name hash. Keep this stateless: no active
  channel color scan and no stored color byte in `ChannelInfo`. The hash must
  mix more than the low three character bits so common short channel names do
  not collapse into the same ink too often. Server remains fixed. Do not use
  PAPER blocks, a
  persistent left margin, or per-line row ownership markers.
- Suppress the old `Switched to ...` notification after a real switch; the
  divider is the switch cue.
- If the previous rendered row was also a channel-context divider and no real
  transcript output has appeared below it, repaint that previous divider row
  instead of adding another one. Track only the expected next row; do not scan
  the screen or keep per-channel row ownership state.
- If a pending divider was created by a short detour away from a channel and the
  user returns to that original channel before any transcript output appears,
  clear the pending divider instead of replacing it with the original channel
  again. This handles `Madrid -> Probando -> Madrid` without leaving two
  `Madrid` dividers around Madrid-only text.
- The divider owns the visible switch timestamp. After drawing it, update
  `last_ts_hour` / `last_ts_minute` so `timestamps=smart` does not repeat a
  topic timestamp immediately below the divider. Topic rendering must not force
  timestamps during JOIN/NAMES bookkeeping; `timestamps=always` remains an
  explicit user setting.
- Do not allocate per-channel scrollback or row-owner arrays in resident RAM
  unless the divider proves insufficient on hardware.
- Skip the switch banner while overlays, pagination, or deferred wrapped output
  are active. Those paths already have ownership and buffer-lifetime contracts.
- Keep IRC routing, `!traffic` / `!notif`, tabswitcher map/key handling, RX
  drain, and channel state unchanged in this visual-only experiment.

Current experimental build:

- Branch: `codex/channel-context-visual`
- Path: `experimentos/channel-context-visual`
- Verification: `git diff --check`; `make NO_COLOR=1`
- Output: `deploy/channel-context-visual/SpecTalkZX.tap`
- Matching runtime files: `deploy/channel-context-visual/SPECTALK.DAT` and
  `deploy/channel-context-visual/SPECTALK.OVL`
- Build size: TAP `36001B`, BSS guard `0xF443 < 0xF500` (`189B` free),
  overlays `1808/1846/1547/1992/2001/1733`.
