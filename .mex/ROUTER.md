# SpecTalkZX Router

## Project State
- Date: 2026-04-22
- Recent task: Search UX follow-up after the SAFE shrink round. Commit `f01a480` cleaned up LIST/WHO messages (`No matches`, `Timeout (incomplete)`), moved server rate-limit NOTICEs into the main area, suppressed stale parser garbage during drain, added a final drain flush, and reverted one `ui_err` consolidation in `cmd_part` after a real-hardware reset. Commit `cd7a41e` put the first LIST/WHO result on the row below `Searching...`, renamed BREAK cancel to `Cancelled (incomplete)` when overflow already happened, and added `post_cancel_quiet` plus a `/search` throttle to avoid flood disconnects after cancel. Prior shrink checkpoint `08729cf` remains the `-63B` SAFE pass.
- Latest committed checkpoint: `cd7a41e` (`Search UX: result on new line, cancel incomplete marker, flood throttle`)
- Build status: local `make` reached the final OK summary on 2026-04-22. All overlays fit; BSS guard OK (180B free).
- Current verified TAP from `make`: `36083B` on 2026-04-22.
- Current overlay sizes: unchanged (`SPCTLK1.OVL` 1385B, `SPCTLK2.OVL` 1968B, `SPCTLK3.OVL` 1591B, `SPCTLK4.OVL` 1796B).
- Audit highlight: the SAFE-only shrink pass in `08729cf` stayed intact; the two Search UX fixes deliberately spent bytes to stabilize LIST/WHO behavior on real hardware and strict IRC networks.

## Resume Here
- No code changes are pending beyond this router refresh. Latest work is committed in `f01a480` and `cd7a41e`.
- Key Search UX changes: `Searching... ` now shares a line only with the final summary while the first actual result starts on a fresh row; server NOTICEs during LIST/WHO print in the main area; stale drain fragments and post-cancel `><` garbage are suppressed; BREAK shows `Cancelled (incomplete)` when overflow already occurred; a short post-cancel quiet window blocks immediate repeat `/search` so the server does not drop us for flood.
- Hardware follow-up worth rechecking: BREAK cancel on `/list` or `/who`, immediate retry showing `Wait a moment before searching again`, rate-limit NOTICE placement on its own row, no stray `><` lines after cancel, and `cmd_part` no longer resetting hardware.
- Pending hardware test checklist (CLAUDE.md): banner, `/connect`, channel join, send/recv, `!about`/`!config`/`!status`, EDIT switcher, esxDOS, 2-3 min stability.
- Next likely work items if development resumes in 48K mode: real auto-reconnect, more useful WHOIS output, less-limited `/list`, or tiny IRC wrappers like `/invite` and `/notice`.

## Patterns
- [`irc-mode-wrapper.md`](patterns/irc-mode-wrapper.md): simple IRC wrappers should prefer passthrough semantics with only the minimum implicit target logic needed for convenience, but query numerics should still produce a visible reply.
- [`help-text-source.md`](patterns/help-text-source.md): command help that ships inside `SPECTALK.DAT` should come from a tracked text source, not from hand-edited local DAT payloads.
- [`notification-builder-helpers.md`](patterns/notification-builder-helpers.md): deduplicate short notification builders by arity, but only keep the helper if `make` proves a net byte win.
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and any prefix routed through `notify2()` must stay out of BPE unless that builder explicitly expands tokens.
- [`disconnect-confirmation-guard.md`](patterns/disconnect-confirmation-guard.md): commands that tear down an active connection should share the same guarded `y/n` confirmation helper instead of open-coding input loops.
- [`local-artifacts-gitignore.md`](patterns/local-artifacts-gitignore.md): local tooling files, backups and build outputs that are not part of the shipped program should be hidden via `.gitignore`, not left as noisy untracked entries.
- [`overlay-about-keepalive.md`](patterns/overlay-about-keepalive.md): if `ABOUT` reuses `ring_buffer`, keepalive must continue without full IRC parsing; accept `PONG` in the lightweight scanner and close the overlay before surfacing timeout UI.
- [`overlay-exit-rx-reset.md`](patterns/overlay-exit-rx-reset.md): overlay exit paths must discard parser/ring state if `ring_buffer` has been reused as overlay code storage.
- [`overlay-resident-string-dedup.md`](patterns/overlay-resident-string-dedup.md): overlays should reuse resident exported string constants when the text is identical, to recover overlay headroom without changing UI or growing TAP.
