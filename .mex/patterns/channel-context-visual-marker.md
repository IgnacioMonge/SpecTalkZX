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

## Implemented Follow-Up: Inverted Channel Label

Implemented 2026-05-11 in `experimentos/channel-separator-polish`.

- `channel_context_ink()` returns just the cue ink. Server labels use
  `ATTR_MSG_SERVER & 7`. Mono themes where `nick_color_mode=0` use
  `ATTR_MSG_CHAN & 7`, avoiding multicolor separators in the Terminal theme.
  Color themes keep the existing stateless cleaned-name hash.
- `channel_context_banner()` splits the attrs. The timestamp and horizontal
  line use cue ink on `ATTR_MAIN_BG` PAPER. The right-side label is inverted:
  label PAPER = cue ink; label INK = main-area background PAPER.
- Keep the existing stateless hash and do not allocate per-channel color
  storage.
- Measured shrink accepted after external review: `channel_context_banner()`
  uses pointer increments for pixel/attr line fill, local HH:MM composition,
  and pointer-based label copy. `channel_context_ink()` uses compact
  `c|=0x20` folding plus a modulo loop. This preserves common alpha-only
  channel color samples but can change deterministic hash color for punctuation
  such as `_`; do not depend on exact hash color for such names.
- Separator display is configurable through the global persisted `divider=`
  config key. Default is ON. `!divider [on|off]` toggles it, `!config` displays
  it, and config save writes it to SD.
- When disabled, no future separators are drawn and any pending divider state is
  cleared, but rows already visible on the screen are left untouched.
- The setting is global only; do not add per-channel separator state.
- Keep this independent from `!traffic` and `!notif`: those affect IRC event
  text and notification routing, not the context divider.
- Treat active-window removal as an active-context change. After
  `remove_channel()` has compacted slots and selected the fallback active slot,
  rerun the divider path. This covers `/part`, `/close`, switcher BACKSPACE,
  self `KICK`, query close on `QUIT`/`401`, and join-error zombie cleanup.
- Before changing/removing the active slot, drain deferred wrapped output while
  no overlay is active, so previous-context wrapped text finishes before the new
  separator is considered.
- If overlay, pagination, or deferred-wrap blocks drawing, keep a single pending
  divider flag and flush it from the main loop when safe. Do not draw while
  blocked and do not add per-channel row ownership.
- `_main_newline()` must clear `channel_context_next_row` when the current
  `main_line` equals it. This catches the bottom-row scroll case: after printing
  a real line on `MAIN_END`, scrolling leaves `main_line` numerically unchanged,
  so `channel_context_banner()` would otherwise think no transcript output
  happened below the previous divider and would reuse/erase the message row.
- When `remove_channel()` compacts slots, adjust `channel_context_anchor_idx`
  like navigation history; if the anchor slot was deleted, invalidate it with
  `0xFF` to prevent false erase-on-return behavior.
- Server-confirmed self `PART` should notify before removing the slot, matching
  local `/part`; otherwise the leave notification is emitted in the fallback
  context.
- Verification needed when implemented: compare at least two channels whose hash
  colors differ, server/query labels, and one non-default theme/background if
  available. Rebuild with `make NO_COLOR=1`; hardware visual check is the real
  gate because this touches attribute composition.

## Mockup TAP

- `tools/channel_separator_preview.c` is a standalone z88dk mockup, not linked
  into SpecTalkZX.
- Output: `deploy/channel-separator-polish/channel_separator_preview.tap`.
- Controls: SPACE cycles pages; any other key exits.
- Pages: one page per current theme (`DEFAULT`, `TERMINAL`, `COMMANDER`) using
  the actual 20 theme attributes loaded from the current `SPECTALK.DAT`.
- The mockup intentionally renders multiple sample separators per page so label
  inversion can be judged across channel-hash colors, server, and query labels.
- Terminal theme is deliberately mono: because the theme's nick and channel INK
  match, the preview keeps channel/query separators green/black instead of using
  the multicolor hash palette. This mirrors `apply_theme()` setting
  `nick_color_mode=0` for mono themes.
- The mockup labels the intended swap explicitly: the inverted label uses the
  channel cue as PAPER and the main background PAPER color as INK.
